import re
from lib.exception import *

CMD_WITH_PARAMETER_PATTERN = re.compile("^\s*([\d\w_]*)\s*([\d\w_]*)\s*,(.*)$")
CMD_WITHOUT_PARAMETER_PATTERN = re.compile("^\s*([\d\w_]*)\s*([\d\w_]*)\s*$")
PARAMETER_PATTERN = re.compile("^\s*([\d\w_\"\']+)\s*=\s*([\d\w_\.\"\']+)\s*$")

   
class BigToolCmd:
    """ This class will parse the command line usr input into three part
    module name, cmmand, parameters
    the first two parts are strings and parameter is one hash and parameter part is optional
    example: zone reload, zone_name=example.com 
    module_name == zone
    command_name == reload
    parameters == {'zone_name' = 'example.com'}
    """
    def __init__(self, cmd):
        self._parse_cmd(cmd)

    def _parse_cmd(self, cmd):    
        cmd_with_parameter = cmd.find(",") != -1
        cmd_group = (cmd_with_parameter and CMD_WITH_PARAMETER_PATTERN or CMD_WITHOUT_PARAMETER_PATTERN).match(cmd)
        if not cmd_group: 
            raise CmdFormatError

        if not cmd_group.group(1):
            raise CmdWithoutModuleName
        self.module_name = cmd_group.group(1)

        if not cmd_group.group(2):
            raise CmdWithoutCommandName(cmd_group.group(1))
        self.command_name = cmd_group.group(2)

        self.parameters = None
        if cmd_with_parameter:
            self._parse_parameters(cmd_group.group(3))

    def _parse_parameters(self, parameters):
        """convert a=b,c=d into one hash """
        key_value_pairs = parameters.split(",")
        if len(key_value_pairs) == 0:
            raise CmdParameterFormatError(self.module_name, self.command_name, "empty parameters")

        self.parameters = {}
        for kv in key_value_pairs:
            key_value = PARAMETER_PATTERN.match(kv)
            if not key_value or not key_value.group(1) or not key_value.group(2):
                raise CmdParameterFormatError(self.module_name, self.command_name, "parameter format should looks like (key = value)")
            self.parameters[key_value.group(1)] = key_value.group(2)

