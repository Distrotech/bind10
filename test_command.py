import unittest
from lib import command
from lib import exception

class TestCommand(unittest.TestCase):

    def testCommandWithoutParameter(self):
        cmd = command.BigToolCmd("zone add")
        assert cmd.module_name == "zone"
        assert cmd.command_name == "add"
        assert cmd.parameters == None

    def testCommandWithParameters(self):
        cmd = command.BigToolCmd("zone add, zone_name = example.com, file = db.example.com.info")
        assert cmd.module_name == "zone"
        assert cmd.command_name == "add"
        assert cmd.parameters["zone_name"] == "example.com"
        assert cmd.parameters["file"] == "db.example.com.info"
    
    def testCommandWithFormatError(self):
        self.assertRaises(exception.CmdFormatError, command.BigToolCmd, "zone=good")
        self.assertRaises(exception.CmdFormatError, command.BigToolCmd, "zo/ne")
        self.assertRaises(exception.CmdWithoutModuleName, command.BigToolCmd, "")

        self.assertRaises(exception.CmdWithoutCommandName, command.BigToolCmd, "zone")
        self.assertRaises(exception.CmdFormatError, command.BigToolCmd, "zone i-s")
        self.assertRaises(exception.CmdFormatError, command.BigToolCmd, "zone i=good")


        self.assertRaises(exception.CmdFormatError, command.BigToolCmd, "zone load load")
        self.assertRaises(exception.CmdFormatError, command.BigToolCmd, "zone file=db.exmaple.info,load")
        self.assertRaises(exception.CmdFormatError, command.BigToolCmd, "zone file=db.exmaple.info load")

        self.assertRaises(exception.CmdParameterFormatError, command.BigToolCmd, "zone load, file=db.exmaple.info load")

if __name__== "__main__":
    unittest.main()
