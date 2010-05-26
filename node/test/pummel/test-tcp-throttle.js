require("../common");
net = require("net");
N = 160*1024; // 30kb

puts("build big string");
var body = "";
for (var i = 0; i < N; i++) {
  body += "C";
}

puts("start server on port " + PORT);

server = net.createServer(function (connection) {
  connection.addListener("connect", function () {
    assert.equal(false, connection.write(body));
    connection.end();
  });
});
server.listen(PORT);


chars_recved = 0;
npauses = 0;


var paused = false;
client = net.createConnection(PORT);
client.setEncoding("ascii");
client.addListener("data", function (d) {
  chars_recved += d.length;
  puts("got " + chars_recved);
  if (!paused) {
    client.pause();
    npauses += 1;
    paused = true;
    puts("pause");
    x = chars_recved;
    setTimeout(function () {
      assert.equal(chars_recved, x);
      client.resume();
      puts("resume");
      paused = false;
    }, 100);
  }
});

client.addListener("end", function () {
  server.close();
  client.end();
});

process.addListener("exit", function () {
  assert.equal(N, chars_recved);
  assert.equal(true, npauses > 2);
});
