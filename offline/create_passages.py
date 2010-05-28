import copy
import os
import random
import simplejson
import time

import couchdb
import flickrapi

import utils

import config
assert config.IMAGE_DIR, "You need to specify a directory to write images to in config.py"
assert os.path.isdir(config.IMAGE_DIR), "Your config.IMAGE_DIR does not specify a valid directory!"

flickr_api_key = '0d5347d0ffb31395e887a63e0a543abe'
_flickr = flickrapi.FlickrAPI(flickr_api_key)

filenames = {'Buddhism':'buddha.json', 'Christianity':'bible.json', 'Hinduism':'vedas.json'}
num_lines = 9
chars_per_line = 25

max_original_width, min_original_width = 3600, 1200
max_original_height, min_original_height = 2400, 800

db = couchdb.Server('http://yorda.cs.northwestern.edu:5984/')['imagination']

def load_passages(filename):
    max_passages = 10 #while testing
    text = simplejson.load(open(filename, 'r'))
    num_passages_yielded = 0
    for book in text:
        lines = []
        curr_line = []
        char_count = 0
        for verse in book['verses']:
            print verse
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
                    curr_line = [word]
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
            #we have a max here, because my network or PIL doesn't like 11k by 8k pictures
            if max_original_width > width > min_original_width and max_original_height > height > min_original_height:
                return url
    return ''
    
def store_passage(religion, passage, passage_num, line_index, image_url):
    file_ending = image_url.rpartition('.')[-1]
    out_filenames = ['%s_%s.%s' % (int(time.time() * 1000), i, file_ending) for i in range(3)]
    utils.crop_images(image_url, *[os.path.join(config.IMAGE_DIR, f) for f in out_filenames])
    
    record = {'religion':religion, 'passage':passage, 'passage_num':passage_num, 'selected_line':line_index, 'images':out_filenames}
    db.save(record)
    print record
    
def run_passage(passage):
    destructable_passage = copy.copy(passage)
    while True:
        line_index = choose_line_index(destructable_passage)
        image_url = find_image(destructable_passage[line_index])
        if not image_url:
            del[destructable_passage[line_index]]
            if not destructable_passage:
                return ('', '')
            continue
        break
    return (line_index, image_url)

def run(religion):
    filename = filenames[religion]
    passages = load_passages(filename)
    for passage_num, passage in enumerate(passages):
        line_index, image_url = run_passage(passage)
        if line_index and image_url:
            store_passage(religion, passage, passage_num, line_index, image_url)
            
def delete_passages():
    for doc_id in db:
        db.delete(db[doc_id])
        print 'deleted one!'
    for filename in os.listdir(config.IMAGE_DIR):
        os.remove(os.path.join(config.IMAGE_DIR, filename))


                                
if __name__ == '__main__':
    # delete_passages()
    # print run('Christianity')
    for doc in db.view('_design/religions/_view/religions', key=["Christianity", 5]):#, endkey=["Christianity", 10000000]):
        print doc
    