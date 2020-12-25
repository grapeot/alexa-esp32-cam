"""
This script helps to generate the gzipped index.html in camera_index.h
"""

content = open('./index.html.gz', 'rb').read()
codes = [hex(n) for n in content]
with open('output.txt', 'w') as fp:
    fp.write('#define index_ov2640_html_gz_len {}\n'.format(len(codes)))
    fp.write('const uint8_t index_ov2640_html_gz[] = {')
    count = 0
    for c in codes:
        if count % 16 == 0:
            fp.write('\n  ')
            count = 0
        count += 1
        fp.write(c)
        fp.write(', ')
    fp.write('\n};')
