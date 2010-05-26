import copy
import random
import simplejson

import flickrapi
flickr_api_key = '0d5347d0ffb31395e887a63e0a543abe'
_flickr = flickrapi.FlickrAPI(flickr_api_key)

num_lines = 9
chars_per_line = 25

def load_passages(filename):
    max_passages = 10 #while testing
    text = simplejson.load(open(filename, 'r'))
    num_passages_yielded = 0
    for book in text:
        lines = []
        curr_line = []
        char_count = 0
        for verse in book['verses']:
            for word in verse.split():
                if char_count + 1 + len(word) < chars_per_line:
                    curr_line.append(word)
                    char_count += len(word)
                else:
                    lines.append(' '.join(curr_line))
                    if len(lines) == num_lines:
                        yield lines
                        if num_passages_yielded > max_passages:
                            return
                        num_passages_yielded += 1
                        lines = []
                    curr_line = []
                    char_count = 0

def choose_line_index(passage):
    return random.randint(0, len(passage) - 1)
    
def _sizeAndURLOfImage(photo_el):
    sizes_el = _flickr.photos_getSizes(photo_id=photo_el.attrib['id'])
    for size in sizes_el.findall(".//size"):# jesus christ
        if size.attrib['label'] == 'Original':
            return ((int(size.attrib['width']), int(size.attrib['height'])), size.attrib['source'])
    return ((-1, -1), '')

def find_image(line):
    for photo in _flickr.walk(text=line, sort='interestingness-desc', per_page='10'):
        (width, height), url = _sizeAndURLOfImage(photo)
        if url:
            if width > 800 and height > 800:
                return url
    return ''
    
def store_passage(passage, line_index, image):
    print '%s goes to %s' % (passage[line_index], image)
    
def run_passage(passage):
    display_passage = copy.copy(passage)
    while True:
        line_index = choose_line_index(passage)
        image = find_image(passage[line_index])
        if not image:
            del[passage[line_index]]
            if not passage:
                return
            continue
        break
    store_passage(passage, line_index, image)

def run(filename):
    passages = load_passages(filename)
    for i, passage in enumerate(passages):
        run_passage(passage)
                                
if __name__ == '__main__':
    print run('vedas.json')
    