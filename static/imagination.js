var socket = null;
var fade_ins = [], fade_outs = [];

var TEXT = {};
TEXT.in_time = 2500;
TEXT.hold_time = 5000;
TEXT.out_time = 5000;
TEXT.delay_zero = 0;
TEXT.delay_one = 3333;
TEXT.delay_two = 6666;
TEXT.opacify_interval = 83;

$(function(foo){
   setupControls();
   setInterval(function() {fade('in');}, TEXT.opacify_interval);
   setInterval(function() {fade('out');}, TEXT.opacify_interval);
});


function fade(in_or_out) {
    var stop_fades = [];
    var now = (new Date).valueOf();
    
    var arr = in_or_out == 'in' ? fade_ins : fade_outs;
    
    $.each(arr, function(i, info) {
        var text = info[0], start_time = info[1], stop_time = info[2];
        if ((start_time < now) && (now < stop_time)) {
            var total_time = stop_time - start_time;
            var elapsed = now - start_time;
            var progress = elapsed / total_time;
            progress = Math.min(progress, 1.0);
            if (in_or_out == 'out') {
                progress = 1 - progress;
            }
            text.css('opacity', progress);
        }
        if (now > stop_time) {
            var progress = in_or_out == 'in' ? 1.0 : 0.0;
            text.css('opacity', progress);
        }
    });
    arr = $.grep(arr, function(info, i) {
        return now < info[2];
    });
    
    // (in_or_out == 'in' ? fade_ins : fade_outs) = arr;
    
    if (in_or_out == 'in') {
        //blech
        fade_ins = arr;
    }
    else {
        fade_outs = arr;
    }
}

function set_fade(text, in_or_out, start_time_offset, fade_time) {
    console.log('start_time_offset is %o, fade_time is %o', start_time_offset, fade_time);
    var fades = in_or_out == 'in' ? fade_ins : fade_outs;
    var curr_time = (new Date).valueOf();
    
    fades.push([text, curr_time + start_time_offset, curr_time + start_time_offset + fade_time]);
}


function ripple(screen_selector) {
    var text_zero = $(screen_selector + ' .text:eq(0)').first();
    var text_one = $(screen_selector + ' .text:eq(1)').first();
    var text_two = $(screen_selector + ' .text:eq(2)').first();
    
    var current_time = (new Date).valueOf();
    
    set_fade(text_zero, 'in', TEXT.delay_zero, TEXT.in_time);
    set_fade(text_zero, 'out', TEXT.delay_zero + TEXT.in_time + TEXT.hold_time, TEXT.out_time);
    
    set_fade(text_one, 'in', TEXT.delay_one, TEXT.in_time);
    set_fade(text_one, 'out', TEXT.delay_one + TEXT.in_time + TEXT.hold_time, TEXT.out_time);
    
    set_fade(text_two, 'in', TEXT.delay_two, TEXT.in_time);
    set_fade(text_two, 'out', TEXT.delay_two + TEXT.in_time + TEXT.hold_time, TEXT.out_time);
}

function onUpdate(data) {
    var data = JSON.parse(data);
    var screen_ = $('#screen-' + data['id']).first();
    if (screen_.length) {
        screen_.css('background-image', 'url("' + data['image_url'] + '")');
        $('.text:eq(0)', screen_).html(data['text0']);
        $('.text:eq(1)', screen_).html(data['text1']);
        $('.text:eq(2)', screen_).html(data['text2']);
        var rippleDelay = parseInt(data.rippleDelay);
        var screen_selector = '#screen-' + data['id'];
        setTimeout(function() {ripple(screen_selector);}, rippleDelay);
    }
}

function resetSocket() {
    if (socket) {
        socket.disconnect();
        socket = null;
    }

    socket = new io.Socket('localhost', {rememberTransport: false, port: 8080});
    socket.connect();
    socket.addEvent('message', onUpdate);
}

function htmlForScreen(class_, id) {
    var ret = '<div class="' + class_ + ' screen" id="screen-' + id + '">'
    console.log("making a screen with id " + id);
    ret += '<div class="text"></div>';
    ret += '<div class="text"></div>';
    ret += '<div class="text"></div>';
    ret += '</div>';
    return ret
}

function setupControls() {
    $('.control#run').click(function () {
        $.get('http://localhost:8080/run');
    })
    $('.control.grid').click(function () {
        resetSocket();
        $('.title').text("Showing all nine screens");
        $('.screens').empty();
        for (var screen_id = 0; screen_id < 9; screen_id++) {
            $('.screens').append(htmlForScreen('grid', screen_id));
        }
    });
    $('.control.column').click(function () {
        resetSocket();
        var column_id = parseInt($(this).attr('id'), 10);
        $('.title').text('Showing column ' + (column_id));
        $('.screens').empty();
        for (var i =0 ; i < 3; i++) {
            var screen_id = column_id + 3 * i;
            $('.screens').append(htmlForScreen('column', screen_id));
        }
    });
    $('.control.single').click(function () {
        resetSocket();
        var screen_id = parseInt($(this).attr('id'), 10);
        $('.title').text('Showing screen ' + (screen_id));
        $('.screens').empty();
        $('.screens').append(htmlForScreen('single', screen_id));
    });
}