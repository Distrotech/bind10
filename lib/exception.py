class BigToolException(Exception):
    """Abstract base class shared by all bigtool exceptions"""
    def __str__(self):
        return "Big tool has problem"
    pass


class CmdFormatError(BigToolException):
    """Command is malformed"""
    def __str__(self):
        return "Command is malformed"

class CmdWithoutModuleName(CmdFormatError):
    """Command is malformed which doesn't have module name"""
    def __str__(self):
        return "Command doesn't have module name"

class CmdWithoutCommandName(CmdFormatError):
    """Command is malformed which donesn't have command name"""
    def __init__(self, module_name):
        super(CmdWithoutCommandName, self).__init__(self)
        self.module_name = module_name

    def __str__(self):
        return "No command name is speicfied for module "\
                + "\"" + self.module_name + "\""

class CmdParameterFormatError(CmdFormatError):
    """Command is malformed which parameter isn't key value pair"""
    def __init__(self, module_name, command_name, error_reason):
        super(CmdParameterFormatError, self).__init__(self)
        self.module_name = module_name
        self.command_name = command_name
        self.error_reason = error_reason

    def __str__(self):
        return "Parameter belongs to command " + "\"" + self.command_name + "\""\
                + " in module " + "\"" + self.module_name + "\""\
                + " has error : " + "\"" + self.error_reason + "\""

class UnknownModule(BigToolException):
    """Command is unknown"""
    def __init__(self, module_name):
        self.module_name = module_name

    def __str__(self):
        return "Module " + "\"" + self.module_name + "\"" + " is unknown"

class UnknownCmd(BigToolException):
    """Command is unknown"""
    def __init__(self, module_name, command_name):
        self.module_name = module_name
        self.command_name = command_name

    def __str__(self):
        return "Module " + "\"" + self.module_name + "\""\
                + " doesn't has command " + "\"" + self.command_name + "\""

class UnknownParameter(BigToolException):
    """The parameter of command is unknown"""
    def __init__(self, module_name, command_name, parameter_name):
        self.module_name = module_name
        self.command_name = command_name
        self.parameter_name = parameter_name

    def __str__(self):
        return "Command " + "\"" + self.command_name + "\""\
                + " in module " + "\"" + self.module_name + "\"" \
                + " doesn't has parameter " + "\"" + self.parameter_name + "\""


class MissingParameter(BigToolException):
    """The parameter of one command is missed"""
    def __init__(self, module_name, command_name, parameter_name):
        self.module_name = module_name
        self.command_name = command_name
        self.parameter_name = parameter_name

    def __str__(self):
        return "Command " + "\"" + self.command_name + "\"" + \
               " in module " + "\"" + self.module_name + "\"" + \
               " is missing parameter " + "\"" + self.parameter_name + "\""

