import http.server
import urllib.parse
import json
import re

URL_PATTERN = re.compile('/([\w]+)(?:/([\w]+))?/?')
MODULE_INFO = [
                [   { 'name' : 'zone', 'desc' : 'manager zones', 'inst' : 'zone_name' },
                    { 'name' : 'add', 'desc' : 'add one zone'}, 
                    { 'name' : 'remove', 'desc' : 'remove one zone'}, 
                    { 'name' : 'list' , 'desc' : 'list all zones'} ],
                [   { 'name' : 'log', 'desc' : 'log module', 'inst' : 'log_module' },
                    { 'name' : 'change_level', 'desc' : 'change log module level'}],
                [   { 'name' : 'boss', 'desc' : 'the boss of bind10', 'inst' : 'module_name'},
                    { 'name' : 'shutdown', 'desc' : 'shut down other module'} ] 
              ]
        
class MyHTTPRequestHandler(http.server.BaseHTTPRequestHandler):
    '''
    Process GET and POST
    '''
        
    def find_module_help(self, name):
        for mod in MODULE_INFO:
            if (mod[0])['name'] == name:
                return mod

        return None

    def find_command(self, mod_name, cmd_name):
        for mod in MODULE_INFO:
            if (mod[0])['name'] == mod_name:
                for cmd in mod[1:]:
                    if (cmd['name'] == cmd_name):
                        return True

        return False

        
    def parse_path(self, path):
        groups = URL_PATTERN.match(path) 
        if not groups:
            return (NULL, NULL)

        module = groups.group(1)
        command = groups.group(2)
        if module:
            module = module.lower()

        if command:
            command = command.lower()
            
        return (module, command)
        
    def do_GET(self):
        mod, cmd = self.parse_path(self.path)
        reply_msg = 'Unknown command'
        if mod == 'help':
            reply_msg = MODULE_INFO
        
        elif cmd == 'help':
            mod_info = self.find_module_help(mod)
            if mod_info:
                reply_msg = mod_info
        
        self.send_response(200)
        self.end_headers()
        self.wfile.write(json.dumps(reply_msg).encode())

        
    def do_POST(self):
        mod, cmd = self.parse_path(self.path)
        reply_msg = 'Unknown command'
        if self.find_command(mod, cmd):
            reply_msg = 'Command is executed'
       
        post_str = str(self.rfile.readline())
        post_str = post_str[ : len(post_str) - 3]
        print("command parameter:%s" % post_str)

        self.send_response(200)
        self.end_headers()
        self.wfile.write(reply_msg.encode())


def run(server_class = http.server.HTTPServer, addr = 'localhost', port = 8080):
    print("control/command module is starting on :%s port:%d" %(addr, port))

    httpd = server_class((addr, port), MyHTTPRequestHandler)
    httpd.serve_forever()


if __name__ == '__main__':
    try:
        run()
    except KeyboardInterrupt:
        print("exit http server")
        

