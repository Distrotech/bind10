from lib import *
from lib.moduleinfo import *

def _prepare_fake_data(bigtool):
    #module zone, has two command : 
    #   zone load, zone_name = "" 
    #   zone set, zone_name = "", master = "", allow_update = ""    
    zone_file_param = ParamInfo(name = "zone_file", 
                                desc = "specify the data source")
    load_cmd = CommandInfo(name = "load", desc = "load one zone")
    load_cmd.add_param(zone_file_param)

    set_param_master = ParamInfo(name = "master", 
                                 desc = "mater of the server", 
                                 optional = True)
                                 
    set_param_allow_update = ParamInfo(name = "allow_update", 
                                       desc = "The addresses allowed to update ",
                                       type = LIST_TYPE,
                                       optional = True)
                                       
    set_cmd = CommandInfo(name = "set", desc = "set attributes of one zone")
    set_cmd.add_param(set_param_master)
    set_cmd.add_param(set_param_allow_update)

    zone_module = ModuleInfo(name = "zone", 
                             inst_name = "zone_name", 
                             inst_type = STRING_TYPE, 
                             inst_desc = "the name of one zone",
                             desc = "manage all the zones")

    zone_module.add_command(load_cmd)
    zone_module.add_command(set_cmd)
    bigtool.add_module_info(zone_module)

    #module log, has two command
    #log change_level, module_name = "", level = ""
    #log close
    level_param = ParamInfo(name = "level", 
                            desc = "module's new log level", 
                            type = INT_TYPE)
    change_level_cmd = CommandInfo(name = "change_level", 
                                   desc = "change module's log level")
    change_level_cmd.add_param(level_param)

    log_close_cmd = CommandInfo(name = "close", 
                                desc = "close log system")
                                
    log_module = ModuleInfo(name = "log", 
                            inst_name = "log_module",                             
                            inst_desc = "log module's name" ,
                            desc = "manage log system")
                            
    log_module.add_command(change_level_cmd)
    log_module.add_command(log_close_cmd)
    bigtool.add_module_info(log_module)
    
    #module config, has one command "reload", and the command has no parameters
    #config reload
    reload_cmd = CommandInfo(name = "reload", 
                             desc = "reload configuration", 
                             need_inst_param = False)

    config_module = ModuleInfo(name = "config", 
                               desc = "config manager")    
    config_module.add_command(reload_cmd)
    bigtool.add_module_info(config_module)
    

if __name__ == '__main__':
    tool = bigtool.BigTool()
    _prepare_fake_data(tool)   
    tool.cmdloop()

