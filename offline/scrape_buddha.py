import BeautifulSoup
import simplejson

import utils as u

def _scrape_book(html):
    ret = []
    doc = BeautifulSoup.BeautifulSoup(html)
    for p in doc.findAll('p'):
        if p.string:
            ret.append(p.string.strip())
    return ret

def scrape_book(url):
    '''Takes a url like http://www.sacred-texts.com/bud/btg/btg56.htm
    Returns a list of strings, each verse-ish.'''
    return u.scrapeWith(url, _scrape_book)
    
def _scrape_index(html):
    ret = []
    base_url = 'http://www.sacred-texts.com/bud/btg/'
    doc = BeautifulSoup.BeautifulSoup(html)
    for i, a_tag in enumerate(doc.findAll('a')):
        if i < 4: #skip garbage, preface, etc.
            continue
        book = {'name':a_tag.string, 'url':base_url + a_tag['href']}
        ret.append(book)
    return ret
    
def scrape_index(url):
    '''Takes a url like http://www.sacred-texts.com/bud/btg/index.htm.
    Returns a list of dictionaries, each with 'name' and 'url' keys (url for scrape_book)'''
    return u.scrapeWith(url, _scrape_index)
    
def scrape_all_and_store():
    '''This is the main function to call.  It will scrape the whole page, and write out buddha.json.
    Buddha.json is a list of dictionaries.  Each dictionary has a 'book_name' and 'verses' keys.
    'verses' is a list of verses.'''
    index_url = 'http://www.sacred-texts.com/bud/btg/index.htm'
    filename = 'buddha.json'
    buddha = []
    for i, book in enumerate(scrape_index(index_url)):
        verses = scrape_book(book['url'])
        print 'Scraping #%s, %s...' % (i, book['name'])
        buddha.append({'book_name':book['name'], 'verses':verses})
    print 'Done scraping, dumping...'
    simplejson.dump(buddha, open(filename, 'w'))
    
    
if __name__ == '__main__':
    scrape_all_and_store()
    # for verse in scrape_book('http://www.sacred-texts.com/bud/btg/btg56.htm'):
        # print verse
    # for book in scrape_index('http://www.sacred-texts.com/bud/btg/index.htm'):
        # print book