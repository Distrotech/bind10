class ParamInfo:
    """The parameter of one command
    each command parameter have four attributes, 
    parameter name, parameter type, parameter value, and parameter description
    """
    def __init__(self, name, type, value, desc):
        self.name = name
        self.type = type
        self.value = value
        self.desc = desc

class CommandInfo:
    """One command which provide by one bind10 module, it has zero or more parameters"""

    def __init__(self, name, desc):
        self.name = name
        self.desc = desc
        self.params = {}

    def add_parameter(self, paraminfo):
        if not isinstance(paraminfo, ParamInfo):
            raise UnknowParamType

        self.params[paraminfo.name] = paraminfo

    def has_parameter_with_name(self, param_name):
        return param_name in self.params

    def get_parameter_with_name(self, param_name):
        return self.params[param_name]

    def get_parameters(self):
        return list(self.params.values())

    def get_parameter_names(self):
        return list(self.params.keys())

 


class ModuleInfo:
    """Define the information of one module, include module name, 
    module supporting commands
    """    
    
    def __init__(self, name, desc):
        self.name = name
        self.desc = desc
        self.commands = {}
        
    def add_command(self, commandInfo):
        if not isinstance(commandInfo, CommandInfo):
            raise UnknownCmdType
        self.commands[commandInfo.name] = commandInfo
        
    def has_command_with_name(self, command_name):
        return command_name in self.commands

    def get_command_with_name(self, command_name):
        return self.commands[command_name]
        
    def get_commands(self):
        return list(self.commands.values())
    
    def get_command_names(self):
        return list(self.commands.keys())
        
