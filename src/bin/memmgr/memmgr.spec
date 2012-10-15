{
  "module_spec": {
    "module_name": "Memmgr",
    "module_description": "Shared memory manager",
    "config_data": [],
    "commands": [
      {
        "command_name": "loadzone",
        "command_description": "(Re)load a specified zone",
        "command_args": [
          {
            "item_name": "class", "item_type": "string",
            "item_optional": true, "item_default": "IN"
          },
	  {
            "item_name": "origin", "item_type": "string",
            "item_optional": false, "item_default": ""
          }
        ]
      }
    ],
    "statistics": []
  }
}
