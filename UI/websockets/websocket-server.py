
#NOTE: You will have to update the ip address in index.html

import tornado.ioloop
import tornado.web
import tornado.websocket
import tornado.template
import threading
import Queue
import serialHandler
#import time 

logMode = False

readQueue = Queue.Queue(maxsize=1)
writeQueue = Queue.Queue(maxsize=1)

class dataOutHtmlHandler(tornado.web.RequestHandler):
  def get(self):
    global logMode
    logMode = False
    loader = tornado.template.Loader(".")
    self.write(loader.load("../dataout.html").generate())

class dataOutJSHandler(tornado.web.RequestHandler):
  def get(self):
    global logMode
    logMode = False
    loader = tornado.template.Loader(".")
    self.write(loader.load("../dataout.js").generate())

class smoothieJSHandler(tornado.web.RequestHandler):
  def get(self):
    global logMode
    logMode = False
    loader = tornado.template.Loader(".")
    self.write(loader.load("../smoothie.js").generate())

class mainCSSHandler(tornado.web.RequestHandler):
  def get(self):
    global logMode
    logMode = False
    loader = tornado.template.Loader(".")
    self.write(loader.load("../main.css").generate())

class calibratePIDHTMLHandler(tornado.web.RequestHandler):
  def get(self):
    global logMode
    logMode = False
    loader = tornado.template.Loader(".")
    self.write(loader.load("../calibratepid.html").generate())

class calibratePIDJSHandler(tornado.web.RequestHandler):
  def get(self):
    global logMode
    logMode = False
    loader = tornado.template.Loader(".")
    self.write(loader.load("../calibratepid.js").generate())


class WSHandler(tornado.websocket.WebSocketHandler):
  run = True  
  def open(self):
    print 'connection opened...'
    #self.write_message("The server says: 'Hello'. Connection was accepted.")
    self.run = True
    self.logFile = open('logFile.log', 'r+')
    if not logMode:
      ui_thread = threading.Thread(target=self.update_ui, args=(readQueue, ))
      ui_thread.daemon = True
      ui_thread.start()
    serial_thread = serialHandler.serialHandler(readQueue, writeQueue)
    serial_thread.daemon = True
    serial_thread.start()

  def on_message(self, message):
    print 'received:', message
    self.logFile.write(message + '\n')
    writeQueue.put(message)
    if logMode:
      update_ui(readQueue)

  def on_close(self):
    print 'connection closed...'
    self.run = False
    writeQueue.put('exit')
    self.logFile.close()

  def update_ui(self, rQ):
    while self.run and not logMode:
      if not rQ.empty():
        val = rQ.get()
        #print val
        self.write_message(str(val))
    if logMode:
      logs = self.logFile.read()
      self.write_message(logs)
      

application = tornado.web.Application([
  (r'/ws', WSHandler),
  (r'/', dataOutHtmlHandler),
  (r'/dataout.html', dataOutHtmlHandler),
  (r'/dataout.js', dataOutJSHandler),
  (r'/smoothie.js', smoothieJSHandler),
  (r'/main.css', mainCSSHandler),
  (r'/calibratepid.html', calibratePIDHTMLHandler),
  (r'/calibratepid.js', calibratePIDJSHandler),
])

if __name__ == "__main__":
  application.listen(9090)
  tornado.ioloop.IOLoop.instance().start()

