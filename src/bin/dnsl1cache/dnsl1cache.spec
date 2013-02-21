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
      },
      {
        "item_name": "enable_scatter_send",
        "item_type": "boolean",
        "item_optional": true,
        "item_default": true
      },
      {
        "item_name": "enable_rr_rotation",
        "item_type": "boolean",
        "item_optional": true,
        "item_default": false
      },
      {
        "item_name": "listen_on",
        "item_type": "list",
        "item_optional": false,
        "item_default": [
          {
            "address": "::",
            "port": 53
          },
          {
            "address": "0.0.0.0",
            "port": 53
          }
        ],
        "list_item_spec": {
          "item_name": "address",
          "item_type": "map",
          "item_optional": false,
          "item_default": {},
          "map_item_spec": [
            {
              "item_name": "address",
              "item_type": "string",
              "item_optional": false,
              "item_default": "::1"
            },
            {
              "item_name": "port",
              "item_type": "integer",
              "item_optional": false,
              "item_default": 53
            }
          ]
        }
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
