# RealidadVirtual
Proyecto final de la cátedra Realidad Virtual de la Facultad de Ingenieria de la UNCuyo

Consiste en añadir a un robot diferencial seguidor de luz previamente contruido y desarrollado, un sistema de cámara PAN-TILT junto con un módulo ESP32 con WiFi incluido. El movimiento de la cámara se realiza remotamente desde una sitio web creado para dicho propósito, en el cual se puede visualizar la imagen recibida de la camara y se puede controlar el movimiento horizontal de la misma mediante 2 servomotores situados en la parte frontal del robot diferencial. Además, se puede aumentar y disminuir la intensidad del brillo del flash que contiene la placa. 

Compuesto por:
  -Carpeta 'hardware': contiene fotos del hardware a utilizar para realizar el proyecto
  -index.html: código con sitio web 'html' mediante el cuál se podrá realizar el control de movimiento de la cámara
  -sliders.js: código javascript para la lectura de valores de ángulo e intensidad de luz, dados por los sliders de la app web
  -web.jpg: captura de pantalla de la app web desarrollada
