      	var webSocketCameraUrl = "ws:\/\/" + window.location.hostname + "/Camera";
        var webSocketServoInputUrl = "ws:\/\/" + window.location.hostname + "/ServoInput";
        var websocketCamera;
        var websocketServoInput;

        function initCameraWebSocket() {
            websocketCamera = new WebSocket(webSocketCameraUrl);
            websocketCamera.binaryType = 'blob';
            websocketCamera.onopen = function (event) { };
            websocketCamera.onclose = function (event) { setTimeout(initCameraWebSocket, 2000); };
            websocketCamera.onmessage = function (event) {
                var imageId = document.getElementById("cameraImage");
                imageId.src = URL.createObjectURL(event.data);
            };
        }

        function initServoInputWebSocket() {
            websocketServoInput = new WebSocket(webSocketServoInputUrl);
            websocketServoInput.onopen = function (event) {
                var panButton = document.getElementById("Pan");
                sendButtonInput("Pan", panButton.value);
                var tiltButton = document.getElementById("Tilt");
                sendButtonInput("Tilt", tiltButton.value);
                var lightButton = document.getElementById("Light");
                sendButtonInput("Light", lightButton.value);
            };
            websocketServoInput.onclose = function (event) { setTimeout(initServoInputWebSocket, 2000); };
            websocketServoInput.onmessage = function (event) { };
        }

        function initWebSocket() {
            initCameraWebSocket();
            initServoInputWebSocket();
        }

        function sendButtonInput(key, value) {
            var data = key + "," + value;
            websocketServoInput.send(data);
        }

        window.onload = initWebSocket;
        document.getElementById("mainTable").addEventListener("touchend", function (event) {
            event.preventDefault()
        });
