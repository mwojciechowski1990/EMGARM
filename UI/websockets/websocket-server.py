
#NOTE: You will have to update the ip address in index.html

import tornado.ioloop
import tornado.web
import tornado.websocket
import tornado.template
import threading
import Queue
import serialHandler
#import time 


readQueue = Queue.Queue()
writeQueue = Queue.Queue()

class MainHandler(tornado.web.RequestHandler):
  def get(self):
    loader = tornado.template.Loader(".")
    self.write(loader.load("index.html").generate())

class WSHandler(tornado.websocket.WebSocketHandler):
  run = True  
  def open(self):
    print 'connection opened...'
    self.write_message("The server says: 'Hello'. Connection was accepted.")
    self.run = True
    ui_thread = threading.Thread(target=self.update_ui, args=(readQueue, ))
    ui_thread.daemon = True
    ui_thread.start()
    serial_thread = serialHandler.serialHandler(readQueue, writeQueue)
    serial_thread.daemon = True
    serial_thread.start()

  def on_message(self, message):
    print 'received:', message
    writeQueue.put(message)

  def on_close(self):
    print 'connection closed...'
    self.run = False
    writeQueue.put('exit')

  def update_ui(self, rQ):
    counter = 0 
    while self.run:
      if not rQ.empty():
        val = rQ.get()
        self.write_message(str(val))
      

application = tornado.web.Application([
  (r'/ws', WSHandler),
  (r'/', MainHandler),
  (r'/tp.html', MainHandler),
  (r"/(.*)", tornado.web.StaticFileHandler, {"path": "./resources"}),
])

if __name__ == "__main__":
  application.listen(9090)
  tornado.ioloop.IOLoop.instance().start()

