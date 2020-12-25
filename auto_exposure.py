import requests
import logging
import cv2
import numpy as np
from time import sleep, time
from datetime import datetime
from os.path import join
from threading import Thread

logging.basicConfig(format='%(asctime)s : %(levelname)s : %(message)s',
                    level=logging.INFO)


class ESP32Camera:
    # completely control by camera, darkest
    EXPOSURE_TIER_0_AEC = 0
    # longest exposure. gain controlled by camera.
    EXPOSURE_TIER_1_AGC = 1
    # gain controlled by us
    EXPOSURE_TIER_2_MANUAL = 2
    EXPOSURE_TIER_MIN = EXPOSURE_TIER_0_AEC
    EXPOSURE_TIER_MAX = EXPOSURE_TIER_2_MANUAL

    # controls how to switch exposure tier
    EXPOSURE_TIER_URL_MAP = {
        EXPOSURE_TIER_0_AEC: [
            '{url}/control?var=aec&val=1'
        ],
        EXPOSURE_TIER_1_AGC: [
            '{url}/control?var=aec&val=0',
            '{url}/control?var=aec_value&val=1200',
            '{url}/control?var=agc&val=1',
        ],
        EXPOSURE_TIER_2_MANUAL: [
            '{url}/control?var=aec&val=0',
            '{url}/control?var=agc&val=0',
            '{url}/control?var=agc_gain&val={gain}',
        ],
    }

    # Controls when to switch to a higher or lower exposure tier
    EXPOSURE_DARK_MEDIAN = 50
    EXPOSURE_BRIGHT_MEDIAN = 200

    EXPOSURE_MAX_GAIN = 31

    def __init__(self, url):
        # Remove the trailing '/'
        self.url = url.rstrip('/')
        # Only used in EXPOSURE_TIER_2_MANUAL
        self.gain = 1
        # Default in ESP32-cam code
        self.exposure_tier = ESP32Camera.EXPOSURE_TIER_1_AGC

    # photo_content is encoded jpg stream
    # returns tier, gain
    def calculate_exposure_tier(self, photo_content):
        np_arr = np.frombuffer(photo_content, np.uint8)
        img = cv2.imdecode(np_arr, cv2.IMREAD_COLOR)
        if img is None:
            logging.error('Cannot parse image from {}.'.format(self.url))
            return self.exposure_tier, self.gain
        median = np.median(img)
        if median >= ESP32Camera.EXPOSURE_BRIGHT_MEDIAN:
            if self.exposure_tier == ESP32Camera.EXPOSURE_TIER_MIN:
                # Scene too bright. We can do nothing here.
                return self.exposure_tier, self.gain
            if self.exposure_tier == ESP32Camera.EXPOSURE_TIER_2_MANUAL:
                # We should try to reduce gain first
                if self.gain > 0:
                    return self.exposure_tier, self.gain - 1
            # Downgrade one tier. We set gain to be 1 here.
            return self.exposure_tier - 1, 1
        elif median <= ESP32Camera.EXPOSURE_DARK_MEDIAN:
            if self.exposure_tier == ESP32Camera.EXPOSURE_TIER_MAX \
                    and self.gain == ESP32Camera.EXPOSURE_MAX_GAIN:
                # Scene too dark. We can do nothing here.
                return self.exposure_tier, self.gain
            if self.exposure_tier < ESP32Camera.EXPOSURE_TIER_MAX:
                # We can upgrade the tier
                return self.exposure_tier + 1, 1
            else:
                # In this case, we can only increase the gain
                return self.exposure_tier, self.gain + 1
        # Do nothing
        return self.exposure_tier, self.gain

    # Actually set the exposure tier on the camera
    def switch_exposure_tier(self, tier, gain):
        for url in ESP32Camera.EXPOSURE_TIER_URL_MAP[tier]:
            requests.get(url.format(url=self.url, gain=gain))

    # Store the photo without decoding when outfn != None
    # Return raw bytes
    def take_photo(self, out_fn=None, adjust_exposure=False):
        try:
            req = requests.get(self.url + '/capture')
        except Exception as e:
            logging.error(e)
            return None
        if req.status_code != 200:
            logging.error('Cannot read from {}'.format(self.url))
            return None
        content = req.content
        if out_fn is not None:
            open(out_fn, 'wb').write(content)
            logging.info('File written to {}.'.format(out_fn))
        if adjust_exposure:
            tier, gain = self.calculate_exposure_tier(content)
            if tier != self.exposure_tier or gain != self.gain:
                self.switch_exposure_tier(tier, gain)
                self.exposure_tier = tier
                self.gain = gain
        return content


class CameraThread(Thread):
    def __init__(self, url, outdir):
        Thread.__init__(self)
        self.cam = ESP32Camera(url)
        self.outdir = outdir

    def run(self):
        while True:
            oldtime = time()
            fn = join(self.outdir, '{}.jpg'.format(datetime.now().strftime(
                '%Y%m%d_%H%M%S')))
            self.cam.take_photo(fn, True)
            while time() - oldtime < 10:
                sleep(1)


if __name__ == '__main__':
    patio_thread = CameraThread('http://esppatio.local:8080', 'patio')
    patio_thread.start()