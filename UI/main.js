   document.getElementById("cnf").checked = true;
   document.getElementById("cf").checked = true;
   document.getElementById("cpido").checked = true;

   var canvasNode = document.getElementById('mycanvas');

   var pw = canvasNode.parentNode.clientWidth;
   var ph = canvasNode.parentNode.clientHeight;

   canvasNode.height = pw * 0.8 * (canvasNode.height / canvasNode.width);
   canvasNode.width = pw * 0.5;
   canvasNode.style.top = (ph - canvasNode.height) / 2 + "px";
   canvasNode.style.left = (pw - canvasNode.width) / 2 + "px";
   // Random data
   var line1 = new TimeSeries();
   var line2 = new TimeSeries();
   var line3 = new TimeSeries();
   setInterval(function() {
       line1.append(new Date().getTime(), Math.random());
       line2.append(new Date().getTime(), Math.random());
       line3.append(new Date().getTime(), Math.random());
   }, 10);

   var cOptions = {
       millisPerPixel: 5,
       interpolation: 'linear',
       grid: {
           strokeStyle: 'rgb(125, 0, 0)',
           fillStyle: 'rgb(60, 0, 0)',
           lineWidth: 0.5,
           millisPerLine: 1000,
           verticalSections: 6
       }
   };

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
       document.getElementById('textRange').value = val;
       canvasNode.width = val;
       canvasNode.style.top = (ph - canvasNode.height) / 2 + "px";
       canvasNode.style.left = (pw - canvasNode.width) / 2 + "px";

   }

   function updatePlotSpeed(val) {
       document.getElementById('textSpeed').value = val;
       smoothie.updateSpeed(val);
       //smoothie.stop();
       //smoothie.start();
   }


   function validateNF() {
       if (document.getElementById('cnf').checked) {
           smoothie.addTimeSeries(line1, {
               strokeStyle: 'rgb(0, 255, 0)',
               lineWidth: 3
           });
       } else {
           smoothie.removeTimeSeries(line1);
       }
   }


   function validateF() {
       if (document.getElementById('cf').checked) {
           smoothie.addTimeSeries(line2, {
               strokeStyle: 'rgb(255, 0, 0)',
               lineWidth: 3
           });
       } else {
           smoothie.removeTimeSeries(line2);
       }
   }

   function validatePIDO() {
       if (document.getElementById('cpido').checked) {
           smoothie.addTimeSeries(line3, {
               strokeStyle: 'rgb(0, 0, 255)',
               lineWidth: 3
           });
       } else {
           smoothie.removeTimeSeries(line3);
       }
   }

