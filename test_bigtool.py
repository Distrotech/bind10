from lib import *

def _prepare_fake_date(bigtool):
    #module zone, has two command : 
    #   zone load, zone_name = "" 
    #   zone set, zone_name = "", attr_name = "", attr_value = ""
    zone_name_param = moduleinfo.ParamInfo("zone_name", "string", "", "specify the zone name")
    zone_file_param = moduleinfo.ParamInfo("zone_file", "string", "", "specify the data source")
    load_cmd = moduleinfo.CommandInfo("load", "load one zone")
    load_cmd.add_parameter(zone_name_param)
    load_cmd.add_parameter(zone_file_param)

    attr_name_param = moduleinfo.ParamInfo("attr_name", "string", "", "specify attribute name")
    attr_value_param = moduleinfo.ParamInfo("attr_value", "string", "", "specify attribute value")
    set_cmd = moduleinfo.CommandInfo("set", "set the master of one zone")
    set_cmd.add_parameter(attr_name_param)
    set_cmd.add_parameter(attr_value_param)
    set_cmd.add_parameter(zone_name_param)
        
    zone_module = moduleinfo.ModuleInfo("zone", "manage all the zones")
    zone_module.add_command(load_cmd)
    zone_module.add_command(set_cmd)
    bigtool.add_module_info(zone_module)

    #module log, has two command
    #log change_level, module_name = "", level = ""
    #log close
    name_param = moduleinfo.ParamInfo("module_name", "string", "", "module name")
    level_param = moduleinfo.ParamInfo("level", "int", "", "module's new log level")
    change_level_cmd = moduleinfo.CommandInfo("change_level", "change module's log level")
    change_level_cmd.add_parameter(name_param)
    change_level_cmd.add_parameter(level_param)

    log_close_cmd = moduleinfo.CommandInfo("close", "close log system")
    log_module = moduleinfo.ModuleInfo("log", "manage log system")
    log_module.add_command(change_level_cmd)
    log_module.add_command(log_close_cmd)
    bigtool.add_module_info(log_module)
    
    #module config, has one command
    #config reload
    config_module = moduleinfo.ModuleInfo("config", "config manager")
    reload_cmd = moduleinfo.CommandInfo("reload", "reload configuration")
    config_module.add_command(reload_cmd)
    bigtool.add_module_info(config_module)
    

if __name__ == '__main__':
    tool = bigtool.BigTool()
    _prepare_fake_date(tool)   
    tool.cmdloop()

