document.getElementById("cIn").checked = true;
document.getElementById("cError").checked = true;
document.getElementById("cPIDout").checked = true;
document.getElementById("cDCCurrent").checked = true;

var canvasNode = document.getElementById('mycanvas');

var pw = canvasNode.parentNode.clientWidth;
var ph = canvasNode.parentNode.clientHeight;

canvasNode.height = pw * 0.75 * (canvasNode.height / canvasNode.width);
canvasNode.width = pw * 0.5;
canvasNode.style.top = (ph - canvasNode.height) / 2 + "px";
canvasNode.style.left = (pw - canvasNode.width) / 2 + "px";
// Random data
var line1 = new TimeSeries();
var line2 = new TimeSeries();
var line3 = new TimeSeries();
var line4 = new TimeSeries();
/*
 setInterval(function() {
 line1.append(new Date().getTime(), Math.random());
 line2.append(new Date().getTime(), Math.random());
 line3.append(new Date().getTime(), Math.random());
 }, 10);
 */
var cOptions = {
    millisPerPixel: 5,
    interpolation: 'linear',
    maxValue: 4096, minValue: 0,
    grid: {
        strokeStyle: 'rgb(125, 0, 0)',
        fillStyle: 'rgb(60, 0, 0)',
        lineWidth: 0.5,
        millisPerLine: 1000,
        verticalSections: 6
    }
};

document.getElementById('txtRange').value = canvasNode.width;
document.getElementById('rRange').value = canvasNode.width;
document.getElementById('txtSpeed').value = cOptions.millisPerPixel;
document.getElementById('rSpeed').value = cOptions.millisPerPixel;

var smoothie = new SmoothieChart(cOptions);
smoothie.addTimeSeries(line1, {
    strokeStyle: 'rgb(0, 255, 0)',
    lineWidth: 3
});
smoothie.addTimeSeries(line2, {
    strokeStyle: 'rgb(255, 0, 0)',
    lineWidth: 3
});
smoothie.addTimeSeries(line3, {
    strokeStyle: 'rgb(0, 0, 255)',
    lineWidth: 3
});

smoothie.streamTo(document.getElementById("mycanvas"), 0);


function updatePlotRange(val) {
    document.getElementById('txtRange').value = val;
    canvasNode.width = val;
    canvasNode.style.top = (ph - canvasNode.height) / 2 + "px";
    canvasNode.style.left = (pw - canvasNode.width) / 2 + "px";

}

function updatePlotSpeed(val) {
    document.getElementById('txtSpeed').value = val;
    smoothie.updateSpeed(val);
}


function validateIN() {
    if (document.getElementById('cIn').checked) {
        smoothie.addTimeSeries(line1, {
            strokeStyle: 'rgb(0, 255, 0)',
            lineWidth: 3
        });
    } else {
        smoothie.removeTimeSeries(line1);
    }
}


function validateError() {
    if (document.getElementById('cError').checked) {
        smoothie.addTimeSeries(line2, {
            strokeStyle: 'rgb(255, 0, 0)',
            lineWidth: 3
        });
    } else {
        smoothie.removeTimeSeries(line2);
    }
}

function validatePIDout() {
    if (document.getElementById('cPIDout').checked) {
        smoothie.addTimeSeries(line3, {
            strokeStyle: 'rgb(0, 0, 255)',
            lineWidth: 3
        });
    } else {
        smoothie.removeTimeSeries(line3);
    }
}

function validateDCCurrent() {
    if (document.getElementById('cDCCurrent').checked) {
        smoothie.addTimeSeries(line4, {
            strokeStyle: 'rgb(255, 255, 255)',
            lineWidth: 3
        });
    } else {
        smoothie.removeTimeSeries(line4);
    }
}

function fPlayPause() {
    var btn = document.getElementById('btPausePlay');
    if (btn.value == 'Pauza') {
        btn.value = 'Odtwarzaj';
        smoothie.stop();
    } else {
        btn.value = 'Pauza';
        smoothie.start();
    }
}


if (!("WebSocket" in window)) {
    alert("Your browser does not support web sockets");
} else {
    setup();
}

function setup() {

// Note: You have to change the host var 
// if your client runs on a different machine than the websocket server

    var host = "ws://localhost:9090/ws";
    var socket = new WebSocket(host);
    // event handlers for websocket
    if (socket) {

        socket.onopen = function () {
            //alert("connection opened....");
        }

        socket.onmessage = function (msg) {
            updatePlot(msg.data);
        }

        socket.onclose = function () {
            //alert("connection closed....");
            //showServerResponse("The connection has been closed.");
        }

    } else {
        console.log("invalid socket");
    }

    function updatePlot(txt) {
        var jsonParams = JSON.parse(txt);
        line1.append(new Date().getTime(), jsonParams.filteredOut);
        line2.append(new Date().getTime(), jsonParams.PIDError);
        line3.append(new Date().getTime(), jsonParams.PIDOut);
        line4.append(new Date().getTime(), jsonParams.DCCurrent);
    }


}


