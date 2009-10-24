import re
import collections
from lib.exception import *


param_name_str = "^\s*(?P<param_name>[\w]+)\s*=\s*"
param_value_str = "(?P<param_value>[\w\.]+)"
param_value_with_quota_str = "[\"\'](?P<param_value>[\w\., ]+)[\"\']"
next_params_str = "(?P<blank>\s*)(?P<comma>,?)(?P<next_params>.*)$"

PARAM_WITH_QUOTA_PATTERN = re.compile(param_name_str + 
                                      param_value_with_quota_str +
                                      next_params_str)
PARAM_PATTERN = re.compile(param_name_str + param_value_str + next_params_str)
                           
# Used for module and command name
MODULE_CMD_NAME_PATTERN = re.compile("^\s*(?P<name>[\w]+)(?P<blank>\s*)(?P<others>.*)$")

class BigToolCmd:
    """ This class will parse the command line usr input into three part
    module name, cmmand, parameters
    the first two parts are strings and parameter is one hash, 
    parameter part is optional
    
    Example: zone reload, zone_name=example.com 
    module == zone
    command == reload
    params == [zone_name = 'example.com']
    """
    
    def __init__(self, cmd):
        self.params = collections.OrderedDict()
        self.module = ''
        self.command = ''
        self._parse_cmd(cmd)

    def _parse_cmd(self, text_str):    
        # Get module name
        groups = MODULE_CMD_NAME_PATTERN.match(text_str)
        if not groups:
            raise CmdModuleNameFormatError
        
        self.module = groups.group('name')
        cmd_str = groups.group('others')
        if cmd_str:
            if not groups.group('blank'):
                raise CmdModuleNameFormatError
        else:            
            raise CmdMissCommandNameFormatError(self.module)
            
        # Get command name
        groups = MODULE_CMD_NAME_PATTERN.match(cmd_str)
        if (not groups):
            raise CmdCommandNameFormatError(self.module)
        
        self.command = groups.group('name')
        param_str = groups.group('others')
        if param_str:
            if not groups.group('blank'):
                raise CmdCommandNameFormatError(self.module)

            self._parse_params(param_str)


    def _parse_params(self, param_text):
        """convert a=b,c=d into one hash """
        
        # Check parameter name "help"
        param = MODULE_CMD_NAME_PATTERN.match(param_text)
        if param and param.group('name') == "help":
            self.params["help"] = "help"
            return
        
        while True:
            if not param_text.strip():
                break
                
            groups = PARAM_PATTERN.match(param_text) or \
                     PARAM_WITH_QUOTA_PATTERN.match(param_text)
            
            if not groups:                
                raise CmdParamFormatError(self.module, self.command)
            else:
                self.params[groups.group('param_name')] = groups.group('param_value')                
                param_text = groups.group('next_params')
                if not param_text or (not param_text.strip()):
                    break

                if not groups.group('blank') and \
                   not groups.group('comma'):
                    raise CmdParamFormatError(self.module, self.command)
                    
                
            
            
            
    

