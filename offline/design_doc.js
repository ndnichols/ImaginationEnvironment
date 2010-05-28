//As of May 27, 2010

{
   "_id": "_design/religions",
   "_rev": "5-d477e4839358b85c28dea23196c1524a",
   "views": {
       "religions": {
           "map": "function(doc) {emit([doc['religion'], doc['passage_num']], doc);}"
       }
   }
}