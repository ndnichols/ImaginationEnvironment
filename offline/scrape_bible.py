import BeautifulSoup
import simplejson

import utils as u

def _scrape_chapter(html):
    ret = []
    doc = BeautifulSoup.BeautifulSoup(html)
    trs = doc.findAll('tr')
    for i, tr in enumerate(trs):
        if i == 0: #first one is section heading or something
            continue
        tds = tr.findAll(lambda tag: tag.p)
        for td in tds:
            verse = ' '.join(td.p.string.split())
            ret.append(verse)
    return ret    

def scrape_chapter(url):
    '''Takes a URL like http://www.htmlbible.com/sacrednamebiblecom/B01C001.htm
    Returns a list of strings, each a verse.'''
    return u.scrapeWith(url, _scrape_chapter)
    
def _scrape_index(html):
    ret = []
    base_url = 'http://www.htmlbible.com/sacrednamebiblecom/'
    doc = BeautifulSoup.BeautifulSoup(html)
    trs = doc.findAll('tr')
    for i, tr in enumerate(trs):
        # print tr
        book = {'urls':[]}
        if i < 2: #first two are old-style HTML
            continue
        for j, td in enumerate(tr.findAll('td')):
            # print td
            if j == 0:
                book['book'] = td.p.string
            else:
                for a in td.findAll('a'):
                    book['urls'].append(base_url + a['href'])
            ret.append(book)
        if book['book'] == 'Revelation':
            break
    return ret
        
    
def scrape_index(url):
    '''Takes a URL like http://www.htmlbible.com/sacrednamebiblecom/index.htm.
    Returns a list of dictionaries, each dictionary has a 'book' (like 'Genesis'), and 
    a 'urls', a list of URLs suitable for passing to scrape_chapter.'''
    return u.scrapeWith(url, _scrape_index)
    
def scrape_all_and_store():
    '''This is the main function to call.  It will scrape the whole page, and write out bible.json.
    bible.json is a list of dictionaries.  Each dictionary has a 'book_name' and 'verses' keys.
    'book_name' is like "Genesis, Chapter 1" 'verses' is a list of verses.'''
    index_url = 'http://www.htmlbible.com/sacrednamebiblecom/index.htm'
    filename = 'bible.json'
    bible = []
    for i, book in enumerate(scrape_index(index_url)):
        for j, url in enumerate(book['urls']):
            verses = scrape_chapter(url)
            bible.append({'book_name':'%s, Chapter %s' % (book['book'], j + 1), 'verses':verses})
            print 'Scraping #%s, %s...' % (i, book['book'])
    print 'Done scraping, dumping...'
    simplejson.dump(bible, open(filename, 'w'))


if __name__ == '__main__':
    # for verse in scrape_chapter('http://www.htmlbible.com/sacrednamebiblecom/B01C001.htm'):
        # print verse
    # for book in scrape_index('http://www.htmlbible.com/sacrednamebiblecom/index.htm'):
        # print book
    scrape_all_and_store()