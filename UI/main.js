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
   setInterval(function() {
       line1.append(new Date().getTime(), Math.random());
       line2.append(new Date().getTime(), Math.random());
   }, 10);

   var smoothie = new SmoothieChart({
       millisPerPixel: 5,
       interpolation: 'linear',
       grid: {
           strokeStyle: 'rgb(125, 0, 0)',
           fillStyle: 'rgb(60, 0, 0)',
           lineWidth: 0.5,
           millisPerLine: 1000,
           verticalSections: 6
       }
   });
   smoothie.addTimeSeries(line1, {
       strokeStyle: 'rgb(0, 255, 0)',
       lineWidth: 3
   });

   smoothie.streamTo(document.getElementById("mycanvas"), 0);


   function updatePlotRange(val) {
       document.getElementById('textRange').value = val;
   }

   function updatePlotSpeed(val) {
       document.getElementById('textSpeed').value = val;
   }
