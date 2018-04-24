
var wsUri = "ws://192.168.43.125:81/";

var websocket;
var commsup = 0;

var workqueue = [];
var workarray = {};
var lastitem;
var RGBHex = "#000000"

var SystemMessageTimeout = null;

$(function () {
    var bCanPreview = true;

    // create canvas and context objects
    var canvas = document.getElementById('picker');
    var ctx = canvas.getContext('2d');

    // drawing active image
    var image = new Image();
    image.onload = function () {
        ctx.drawImage(image, 0, 0, image.width, image.height); // draw the image on the canvas
    }

    // select desired colorwheel
    var imageSrc = 'images/MagicalLampLogo.png';
    image.src = imageSrc;

    $('#picker').mousemove(function (e) { // mouse move handler
        if (bCanPreview) {
            // get coordinates of current position
            var canvasOffset = $(canvas).offset();
            var canvasX = Math.floor(e.pageX - canvasOffset.left);
            var canvasY = Math.floor(e.pageY - canvasOffset.top);

            // get current pixel
            var imageData = ctx.getImageData(canvasX, canvasY, 1, 1);
            var pixel = imageData.data;

            var dColor = pixel[2] + 256 * pixel[1] + 65536 * pixel[0];
            RGBHex = '#' + ('0000' + dColor.toString(16)).substr(-6);
            sendRGB();
        }
    });
});

function MagicCheckBoxHandler(cb) {
    if (cb.checked) {
        QueueOperation("ME");
    }
    else {
        QueueOperation("MD");
    }
}

function IssueSystemMessage(msg) {
    var elem = $("#SystemMessage");
    elem.hide();
    elem.html("<font size=+2>" + msg + "</font>");
    elem.fadeIn('slow');
    if (SystemMessageTimeout != null) clearTimeout(SystemMessageTimeout);
    SystemMessageTimeout = setTimeout(function () { SystemMessageTimeout = null; $("#SystemMessage").fadeOut('slow') }, 3000);
}

function QueueOperation(command, callback) {
    if (workarray[command] == 1) {
        return;
    }

    workarray[command] = 1;
    var vp = new Object();
    vp.callback = callback;
    vp.request = command;
    workqueue.push(vp);
}

function init() {
    Ticker();

    RGBTickerStart();
}

window.addEventListener("load", init, false);

function StartWebSocket() {
    if (websocket) websocket.close();
    workarray = {};
    workqueue = [];
    lastitem = null;
    websocket = new WebSocket(wsUri, ['arduino']);
    websocket.onopen = function (evt) { onOpen(evt) };
    websocket.onclose = function (evt) { onClose(evt) };
    websocket.onmessage = function (evt) { onMessage(evt) };
    websocket.onerror = function (evt) { onError(evt) };
}

function onOpen(evt) {
    websocket.send('ping');
}

function onClose(evt) {
    commsup = 0;
}

var msg = 0;
var tickmessage = 0;
var lasthz = 0;
var time_since_hz = 0;
function Ticker() {
    setTimeout(Ticker, 1000);

    lasthz = (msg - tickmessage);
    tickmessage = msg;
    if (lasthz == 0) {
        time_since_hz++;
        if (time_since_hz > 3) {
            if (commsup != 0) IssueSystemMessage("Connection Lost...");
            commsup = 0;
            StartWebSocket();
        }
    }
    else {
        time_since_hz = 0;
    }
}

function onMessage(evt) {
    msg++;
    if (commsup != 1) {
        commsup = 1;
        IssueSystemMessage("Connected!");
    }

    if (lastitem) {
        if (lastitem.callback) {
            lastitem.callback(lastitem, evt.data);
            lastitem = null;
        }
    }

    if (workqueue.length) {
        var elem = workqueue.shift();
        delete workarray[elem.request];

        if (elem.request) {
            doSend(elem.request);
            lastitem = elem;
            return;
        }
    }

    doSend('wx'); //Request RSSI.
}

function sendRGB() {
    console.log('RGB: ' + RGBHex);
    if (!document.getElementById('MagicCheckBox').checked) {
        QueueOperation(RGBHex);
    }
}

function onError(evt) {
    commsup = 0;
}

function doSend(message) {
    websocket.send(message);
}

function RGBTicker() {
    sendRGB();
    setTimeout(RGBTicker, 500);
}

function RGBTickerStart() {
    RGBTicker();
}