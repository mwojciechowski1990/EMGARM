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
    var txtLogs = document.getElementById("txtLogs");

    // event handlers for websocket
    if (socket) {

        socket.onopen = function () {
            //alert("connection opened....");
        }

        socket.onmessage = function (msg) {
            updateLog(msg.data);
        }

        socket.onclose = function () {
            //alert("connection closed....");
            //showServerResponse("The connection has been closed.");
        }

    } else {
        console.log("invalid socket");
    }

    function updateLog(txt) {
        txtLogs.innerHTML = txt;
    }


}

