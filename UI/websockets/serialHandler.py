import threading
import time
import serial

class serialHandler(threading.Thread):
  """
      A thread class that will count a number, sleep and output that number
  """
  def __init__ (self, rQ, wQ):
    self.inQ = wQ
    self.outQ = rQ
    threading.Thread.__init__ (self)

  def run(self):
    counter = 0
    self.ser = serial.Serial('/dev/ttyACM0', 57600, timeout=1)
    while True:
      #counter = counter + 1
      line = self.ser.readline()
      #print line
      self.outQ.put(line)
      if not self.inQ.empty():
        val = self.inQ.get()
        if val == 'exit':
          print 'Exiting'
          break
        self.ser.write(val)
      #time.sleep(1)
    self.ser.close()
