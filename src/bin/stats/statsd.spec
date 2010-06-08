{
  "module_spec": {
    "module_name": "statsd",
    "module_description": "statistics",
    "config_data": [
      {
        "item_name": "output_path",
        "item_type": "string",
        "item_optional": False,
        "item_default": "/tmp/stats.xml"
      },
      {
        "item_name": "output_interval",
        "item_type": "integer",
        "item_optional": False,
        "item_default": 10
      },
      {
        "item_name": "output_generation",
        "item_type": "integer",
        "item_optional": False,
        "item_default": 100
      }
    ],
    "commands": [
      {
        "command_name": "shutdown",
        "command_description": "shutdown",
        "command_args": []
      },
      {
        "command_name": "clear_counters",
        "command_description": "Clear Counters",
        "command_args": []
      },
      {
        "command_name": "show_statistics",
        "command_description": "example",
        "command_args": []
      },
    ]
  }
}


