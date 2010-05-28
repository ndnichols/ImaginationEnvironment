var sys = require("sys"),
    http = require("http"),
    url = require("url"),
    path = require("path"),
    fs = require("fs"),
    events = require("events"),
    io = require('./server/socket.io'),
    screens = require("./server/screens");

screens.setup();

var mimetypes = {'.swf':'application/x-shockwave-flash', '.js':'text/javascript', 'html':'text/html', '.css':'text/css', '.jpg':'image/jpeg', 'jpeg':'image/jpeg', '.png':'image/png', '.gif':'image/gif'};
var static_dir = '/Users/nate/Programming/ImaginationEnvironment/static'

function onScreenUpdate(screen_) {
    listener.broadcast(JSON.stringify(screen_));
}

screens.get_screen_emitter().addListener('screen', onScreenUpdate);
    
var server = http.createServer(function(req, res) {
    var parsed_req = url.parse(req.url, true);
    var path = parsed_req.pathname;
    sys.puts(path);
	switch (path){
		case '/':
		    var filename = static_dir + '/imagination.html'
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
					var filetype = mimetypes[path.substr(-4)];
					var binary = /^(application|image)/.test(filetype);
					sys.puts(filetype + " is binary: " + binary);
					res.writeHead(200, {'Content-Type': filetype});
					sys.puts("going to read " + (static_dir + path));
					res.write(fs.readFileSync(static_dir + path, binary ? 'binary' : 'utf8'), binary ? 'binary' : 'utf8');
					res.end();
				} catch(e){
				    sys.puts('error!');
					send404(res); 
				}				
				break;
			}
			send404(res);
			break;
	}
    
});

server.listen(8080);

var listener = io.listen(server, {});

sys.puts("Server running at http://localhost:8080/");