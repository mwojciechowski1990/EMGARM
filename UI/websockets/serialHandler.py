import threading
import time

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
    while True:
      counter = counter + 1
      self.outQ.put(counter)
      print 'Writing'
      if not self.inQ.empty():
        val = self.inQ.get()
        print 'ser received:', val
        if val == 'exit':
          print 'Exiting'
          break
      time.sleep(1)
