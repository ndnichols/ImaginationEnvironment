import BeautifulSoup
import simplejson
import re

import utils as u

verse_start_regex = re.compile(r'^\s*\d+\.?(.*)')

def _scrape_hymn(html):
    ret = []
    doc = BeautifulSoup.BeautifulSoup(html)
    h3 = doc.body.h3
    p = h3.findNextSibling(True)
    curr_verse = ''
    for tag in p:
        text = tag.string
        if text is not None:
            match = verse_start_regex.match(text)
            if match is not None:
                if curr_verse:
                    ret.append(curr_verse.strip())
                curr_verse = match.groups()[0]
            else:
                curr_verse += text
    return ret
    

def scrape_hymn(url):
    '''Takes a url like http://www.sacred-texts.com/hin/rigveda/rv05055.htm.
    Returns a list of strings, each a verse-ish'''
    return u.scrapeWith(url, _scrape_hymn)
    
def _scrape_book(html):
    ret = []
    base_url = 'http://www.sacred-texts.com/hin/rigveda/'
    doc = BeautifulSoup.BeautifulSoup(html)
    for a_tag in doc.findAll(lambda tag: tag.name == 'a' and tag.string and tag.string.startswith('HYMN')):
        try:
            ret.append({'name':a_tag.string.split('.')[1].strip(), 'url':base_url + a_tag['href']})
        except IndexError: #Some hymns don't have periods or something?  Oh well, we aren't hurting for vedas
            pass
    return ret
    
def scrape_book(url):
    '''Takes a url like http://www.sacred-texts.com/hin/rigveda/rvi01.htm.
    Returns a list of dictionaries, each with a 'name' and 'url' (url is for
    scrape_hymn)'''
    return u.scrapeWith(url, _scrape_book)
    
def scrape_all_and_store():
    '''This is the main function to call.  It will scrape the whole page, and write out vedas.json
    vedas.json is a list of dictionaries.  Each dictionary has a 'book_name' and 'verses' keys.
    'verses' is a list of verses.'''
    root_url = 'http://www.sacred-texts.com/hin/rigveda/rvi%02d.htm'
    filename = 'vedas.json'
    vedas = []
    for i in range(1, 11):
       index_url = root_url % i
       for j, book in enumerate(scrape_book(index_url)):
           print 'Scraping #%s, %s...' % (j, book['name'])
           verses = scrape_hymn(book['url'])
           vedas.append({'book_name':book['name'], 'verses':verses})
    simplejson.dump(vedas, open(filename, 'w'))


if __name__ == '__main__':
    # print scrape_hymn('http://www.sacred-texts.com/hin/rigveda/rv05055.htm')
    # print scrape_book('http://www.sacred-texts.com/hin/rigveda/rvi01.htm')
    scrape_all_and_store()