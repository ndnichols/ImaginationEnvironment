var sys = require("sys"),
    http = require("http"),
    url = require("url"),
    path = require("path"),
    fs = require("fs"),
    events = require("events"),
    io = require('./server/socket.io'),
    screens = require("./server/screens");

screens.setup();

function onScreenUpdate(screen_) {
    listener.broadcast(JSON.stringify(screen_));
}

screens.get_screen_emitter().addListener('screen', onScreenUpdate);
    
var server = http.createServer(function(req, res) {
    var parsed_req = url.parse(req.url, true);
    var path = parsed_req.pathname;
	switch (path){
		case '/':
		    var filename = __dirname + '/imagination.html'
    		res.writeHead(200, {'Content-Type': 'text/html'});
    		res.write(fs.readFileSync(filename, 'utf8'), 'utf8');
    		res.end();
    		break;
		
		case '/run':
            sys.puts("Running!");
            screens.run();
            break;
            
		default:
			if (/\/|\.(js|html|swf)$/.test(path)){
				try {
					var swf = path.substr(-4) == '.swf';
					res.writeHead(200, {'Content-Type': swf ? 'application/x-shockwave-flash' : ('text/' + (path.substr(-3) == '.js' ? 'javascript' : 'html'))});
					res.write(fs.readFileSync(__dirname + path, swf ? 'binary' : 'utf8'), swf ? 'binary' : 'utf8');
					res.end();
				} catch(e){ 
					send404(res); 
				}				
				break;
			}

			send404(res);
			break;
	}
    
})

server.listen(8080);

var listener = io.listen(server, {});

sys.puts("Server running at http://localhost:8080/");