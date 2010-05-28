import hashlib
import random
import re
import os
import shutil
import time
import unicodedata
import urllib

import BeautifulSoup
import Image

import config

assert config.WEB_CACHE_DIR, "You need to put a variable in your config.py that points to a directory you don't mind getting filled with pages"
assert os.path.isdir(config.WEB_CACHE_DIR), "config.WEB_CACHE_DIR is not pointing to a directory!"
_memory_cache = {}

target_image_width = 1024
target_image_height = 768

def _getFile(url, cachedFile=True, return_filename=False):
    """Does some caching too, not threadsafe, nothing fancy, but MC and RT are slow as all hell."""
    assert url, "WHY are you trying to load an empty string url?!?!  Nothing good will come of this!  In fact, I will assure that! %s" % (url)
    md5 = hashlib.md5(url).hexdigest()
    filename = os.path.join(config.WEB_CACHE_DIR, md5)
    if os.path.exists(filename) and cachedFile:
        ret = open(filename, 'r').read()
    else:
        opener = urllib.FancyURLopener()
        ret = opener.open(url).read()
        o = open(filename, 'w')
        o.write(ret)
        o.close()
    if return_filename:
        return filename
    else:
        return ret
    
def _clearFile(url):
    """This clears the file at url out of the cache, if it was in there.  You can use this for testing stuff, or clearing 
    munged stuff. """
    md5 = hashlib.md5(url).hexdigest()
    filename = os.path.join(config.WEB_CACHE_DIR, md5)
    if os.path.exists(filename):
        os.remove(filename)
        
def _getMemory(url):
    assert url, "WHY are you trying to load an empty string url?!?!  Nothing good will come of this!  In fact, I will assure that! %s" % (url)
    md5 = hashlib.md5(url).hexdigest()
    if md5 in _memory_cache:
        return _memory_cache[md5]
    # html = pyU.GetFile(url)
    opener = urllib.FancyURLopener()
    html = opener.open(url).read()
    _memory_cache[md5] = html
    return html

def _clearMemory(url):
    md5 = hashlib.md5(url).hexdigest()
    if md5 in _memory_cache:
        del _memory_cache[md5]
        
def CacheOnDisk(yes_or_no):
    """This is kind of weird that the caching stuff is just floating around in movieutils, but whatever.  
    This switches to a 'temporary cache' instead of the one that actually stores on disk.  So this only caches
    in memory.  It's used by the testcases to make sure that 1)We're not just rescraping files stored on disk
    (which presumably it would never fail at) and 2)We're doing as much caching during the testing because it
    takes forever.  Buy some more servers, RT!"""
    global GetFile, ClearFile
    if yes_or_no:
        GetFile = _getFile
        ClearFile = _clearFile
    else:
        GetFile = _getMemory
        ClearFile = _clearMemory

def GetCacheOnDisk():
    return GetFile is _getFile

GetFile = _getFile
ClearFile = _clearFile

def scrapeWith(url, func):
    '''Im actually jonesing for Objc style arguments here, but I do not have them.  The idea is that we are scraping URL with
    function func, and then it returns the result of func.  So we load html from url, pass it to func, and then return funcs ret.
    The hook is that the function also catches errors, and blows away the cache if they happen.'''
    tries = 0
    html = GetFile(url)
    return func(html)
    while tries < 3:
        tries += 1
        try:
            html = GetFile(url)
        except (IOError, ), e:
            print 'Got a big IOError trying to MovieName %s and %s and %s' % (t, foo, bar)
            raise
        try:
            ret = func(html)
        except Exception, e:
            print "BLAST! Had an error %s trying to use %s to scrape %s" % (e, func.func_name, url)
            ClearFile(url)
            time.sleep(5)
        else:
            return ret

def _replace(match):
    """Does the replace deal."""
    match = match.groups()[0]
    if match in _html_escapes:
        ret = _html_escapes[match]
    else:
        ret = unicode(chr(int(match[1:])), 'latin-1')
    return ret

def unescape(s):
    ret = ''
    ret = _html_regex.sub(_replace, s)
    return ret
    
_char_map = {8722: '-', 8211: '-', 8212: '-', 8213: '-', 8216: "'", 8217: "'", 8218: ',', 8220: '"', 8221: '"', 8230: '...', 187: '>>', 7789: 't', 171: '<<', 173: '-', 180: "'", 699: "'", 7871: 'e', 192: 'A', 193: 'A', 194: 'A', 195: 'A', 196: 'A', 197: 'A', 198: 'Ae', 199: 'C', 200: 'E', 201: 'E', 202: 'E', 203: 'E', 204: 'I', 7885: 'o', 206: 'I', 205: 'I', 208: 'D', 209: 'N', 210: 'O', 211: 'O', 212: 'O', 213: 'O', 214: 'O', 215: 'x', 216: 'O', 217: 'U', 218: 'U', 207: 'I', 220: 'U', 221: 'Y', 223: 'S', 224: 'a', 225: 'a', 226: 'a', 227: 'a', 228: 'a', 229: 'a', 230: 'ae', 231: 'c', 232: 'e', 233: 'e', 234: 'e', 235: 'e', 236: 'i', 237: 'i', 238: 'i', 239: 'i', 240: 'o', 241: 'n', 242: 'o', 243: 'o', 244: 'o', 245: 'o', 246: 'o', 247: '/', 248: 'o', 249: 'u', 250: 'u', 251: 'u', 252: 'u', 253: 'y', 255: 'y', 256: 'A', 257: 'a', 259: 'a', 261: 'a', 263: 'c', 268: 'C', 269: 'c', 279: 'e', 281: 'e', 283: 'e', 287: 'g', 219: 'U', 298: 'I', 299: 'i', 304: 'I', 305: 'i', 322: 'l', 324: 'n', 332: 'O', 333: 'o', 335: 't', 337: 'o', 339: 'oe', 345: 'r', 346: 'S', 347: 's', 351: 's', 352: 'S', 353: 's', 355: 'c', 363: 'u', 367: 'u', 378: 'z', 379: 'Z', 381: 'Z', 382: 'z', 924: 'M', 451: '!'}
def toascii(text):
    if type(text) is not unicode:
        try:
            text = unicode(text, "utf-8", 'ignore')
        except TypeError, e:
            pass
        text = unicodedata.normalize('NFKD', text)
    ret = [c if ord(c) < 128 else _char_map.get(ord(c), '') for c in text]
    ret = ''.join(ret)
    return ret
    
    
def crop_images(in_url, *out_filenames):
    '''Takes a filename for the image to crop, and a list of filenames to store cropped versions in.
    Returns True or False for success'''
    img_filename = GetFile(in_url, return_filename=True)
    
    if True:#'jpeg_decoder' in dir(Image.core):
        try:
            image = Image.open(img_filename)
        except IOError, e:
            return False
        for out_filename in out_filenames:
            max_scale = min(image.size[0] / float(target_image_width), image.size[1] / float(target_image_height))
            scale = random.uniform(1.0, max_scale)
            print scale
            crop_width = scale * target_image_width#random.randint(target_image_width, image.size[0] - 1)
            crop_height = scale * target_image_height#random.randint(target_image_height, image.size[1] - 1)
            
            crop_x = random.randint(0, int(image.size[0] - crop_width - 1))
            crop_y = random.randint(0, int(image.size[1] - crop_height - 1))
            print crop_width, crop_height, crop_x, crop_y
            crop = (crop_x, crop_y, crop_x + crop_width, crop_y + crop_height)
            region = image.crop(crop).resize((target_image_width, target_image_height))
            region.save(out_filename, dpi=(24, 24))
    else:
        print 'No jpeg, just copying'
        for out_filename in out_filenames:
            shutil.copyfile(img_filename, out_filename)
    return True
            
if __name__ == '__main__':
    print crop_images('http://stereo.gsfc.nasa.gov/img/spaceweather/preview/tricompSW.jpg', '/Users/nate/Desktop/out1.jpg', '/Users/nate/Desktop/out2.jpg', '/Users/nate/Desktop/out3.jpg')
