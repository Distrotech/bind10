{
  "module_spec": {
    "module_name": "DNSL1Cache",
    "module_description": "Ultra-fast DNS cache (only) server",
    "config_data": [
      {
        "item_name": "cache_file",
        "item_type": "string",
        "item_optional": true,
        "item_default": ""
      }
    ],
    "commands": [
      {
        "command_name": "shutdown",
        "command_description": "Shut down the DNS cache server",
        "command_args": [
          {
            "item_name": "pid",
            "item_type": "integer",
            "item_optional": true
          }
        ]
      }
    ],
    "statistics": []
  }
}
