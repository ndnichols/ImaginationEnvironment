var sys = require("sys"),
    events = require("events");
var emitter = new events.EventEmitter();

var screens = [];

var christianImages = ['http://kiddmillennium.files.wordpress.com/2009/02/jesus-thumps-up1.jpg?w=400&h=400', 'http://scrapetv.com/News/News%20Pages/Entertainment/Images/jesus.jpg', 'http://www.morethings.com/god_and_country/jesus/children-jesus-170.gif', 'http://holyhell.files.wordpress.com/2009/02/jesus-christ-w-lamb.jpg', 'http://faithfool.files.wordpress.com/2007/07/white-jesus.jpg'];
var hinduismImages = ['http://www.indianchild.com/images/hindu_god_ram.jpg', 'http://momstinfoilhat.files.wordpress.com/2009/08/hindu-gods-kali.jpg', 'http://donyes.typepad.com/.a/6a00e554f2e0b988340120a4f85d81970b-800wi', 'http://www.moonbattery.com/archives/hindu-god.jpg', 'https://s3.amazonaws.com:443/cs-vannet/CommunityServer.Components.PostAttachments/00/00/00/16/43/stories+of+krishna+the+adventures+of+a+hindu+god+1.jpg?AWSAccessKeyId=0TTXDM86AJ1CB68A7P02&Expires=1274297361&Signature=7wQIrRGWbr%2bXqR%2b0RMjfwvqswl0%3d'];
var buddhismImages = ['http://thepopeofpentecost.files.wordpress.com/2010/02/buddhism.jpg', 'http://erinsaley.files.wordpress.com/2009/02/buddah1.jpg', 'http://religions.iloveindia.com/images/buddhism.jpg', 'http://sundaytimes.lk/070527/images/mumbai.jpg', 'http://1.bp.blogspot.com/_WUdmYiDgMdo/S1cTuCRvxuI/AAAAAAAAAQ0/AFJmfBSYlA8/s400/Buddhism.jpg'];

exports.get_screen_emitter = function () {return emitter};
exports.setup = function() {
    for (var i = 0; i < 9; i++) {
        var screen_ = {};
        screen_.id = i;
        screen_.image_url = 'http://infolab.northwestern.edu/media/uploaded_images/featured_illumination.jpg';
        screen_.text0 = 'Welcome to';
        screen_.text1 = 'the';
        screen_.text2 = 'Imagination Environment';
        screens.push(screen_);
    }    
}

function updateScreen(screen_index) {
    emitter.emit('screen', screens[screen_index]);
}

exports.run = function() {
    for (var i = 0; i < 9; i++) {
        updateScreen(i);
    }
    runChristianity();
    setTimeout(runHinduism, 10000);
    setTimeout(runBuddhism, 20000);
}

function randElement(arr) {
    var index = Math.floor(Math.random() * arr.length);
    return arr[index];
}

function nextChristian() {
    sys.puts("Next Christian!");
    screens[0].image_url = randElement(christianImages);
    screens[0].rippleDelay = 0;
    updateScreen(0);
    screens[3].image_url = randElement(christianImages);
    screens[3].rippleDelay = 10000;
    updateScreen(3);
    screens[6].image_url = randElement(christianImages);
    screens[6].rippleDelay = 20000;
    updateScreen(6);
}

function nextHinduism() {
    sys.puts("Next Hindu!");
    screens[1].image_url = randElement(buddhismImages);
    screens[1].rippleDelay = 0;
    updateScreen(1);
    screens[4].image_url = randElement(buddhismImages);
    screens[4].rippleDelay = 10000;
    updateScreen(4);
    screens[7].image_url = randElement(buddhismImages);
    screens[7].rippleDelay = 20000;
    updateScreen(7);
}

function nextBuddhism() {
    sys.puts("Next Buddhism!");
    screens[2].image_url = randElement(hinduismImages);
    screens[2].rippleDelay = 0;
    updateScreen(2);
    screens[5].image_url = randElement(hinduismImages);
    screens[5].rippleDelay = 10000;
    updateScreen(5);
    screens[8].image_url = randElement(hinduismImages);
    screens[8].rippleDelay = 20000;
    updateScreen(8);
}

function runChristianity() {
    setInterval(nextChristian, 30000);
    nextChristian();
}

function runHinduism() {
    setInterval(nextHinduism, 30000);
    nextHinduism();
}

function runBuddhism() {
    setInterval(nextBuddhism, 30000);
    nextBuddhism();
}