var database__protocol_8h =
[
    [ "kcenon::database::protocol::message_header", "structkcenon_1_1database_1_1protocol_1_1message__header.html", "structkcenon_1_1database_1_1protocol_1_1message__header" ],
    [ "kcenon::database::protocol::connect_request", "structkcenon_1_1database_1_1protocol_1_1connect__request.html", "structkcenon_1_1database_1_1protocol_1_1connect__request" ],
    [ "kcenon::database::protocol::connect_response", "structkcenon_1_1database_1_1protocol_1_1connect__response.html", "structkcenon_1_1database_1_1protocol_1_1connect__response" ],
    [ "kcenon::database::protocol::query_request", "structkcenon_1_1database_1_1protocol_1_1query__request.html", "structkcenon_1_1database_1_1protocol_1_1query__request" ],
    [ "kcenon::database::protocol::query_response", "structkcenon_1_1database_1_1protocol_1_1query__response.html", "structkcenon_1_1database_1_1protocol_1_1query__response" ],
    [ "kcenon::database::protocol::transaction_request", "structkcenon_1_1database_1_1protocol_1_1transaction__request.html", "structkcenon_1_1database_1_1protocol_1_1transaction__request" ],
    [ "kcenon::database::protocol::transaction_response", "structkcenon_1_1database_1_1protocol_1_1transaction__response.html", "structkcenon_1_1database_1_1protocol_1_1transaction__response" ],
    [ "kcenon::database::protocol::error_response", "structkcenon_1_1database_1_1protocol_1_1error__response.html", "structkcenon_1_1database_1_1protocol_1_1error__response" ],
    [ "kcenon::database::protocol::protocol_serializer", "classkcenon_1_1database_1_1protocol_1_1protocol__serializer.html", "classkcenon_1_1database_1_1protocol_1_1protocol__serializer" ],
    [ "message_type", "database__protocol_8h.html#ad8c43c9ed6f095c8422ee0db2e5adc24", [
      [ "CONNECT_REQUEST", "database__protocol_8h.html#ad8c43c9ed6f095c8422ee0db2e5adc24ac8b1ac17ffa9f347a03de40402930b00", null ],
      [ "CONNECT_RESPONSE", "database__protocol_8h.html#ad8c43c9ed6f095c8422ee0db2e5adc24a4438529b495e7eb6c5f141870544b4f5", null ],
      [ "DISCONNECT", "database__protocol_8h.html#ad8c43c9ed6f095c8422ee0db2e5adc24add7cd0eb57db08d4f98abc48e5593462", null ],
      [ "PING", "database__protocol_8h.html#ad8c43c9ed6f095c8422ee0db2e5adc24ae07ff41a486c27c095a15898dcca34d1", null ],
      [ "PONG", "database__protocol_8h.html#ad8c43c9ed6f095c8422ee0db2e5adc24a15c94917c8795679ecb650ed760c27f0", null ],
      [ "QUERY_REQUEST", "database__protocol_8h.html#ad8c43c9ed6f095c8422ee0db2e5adc24a98f9b844db5237c70eb29f891b0a1de7", null ],
      [ "QUERY_RESPONSE", "database__protocol_8h.html#ad8c43c9ed6f095c8422ee0db2e5adc24ad772a25ec0f32e5b2683f6ac00d24249", null ],
      [ "BEGIN_TRANSACTION", "database__protocol_8h.html#ad8c43c9ed6f095c8422ee0db2e5adc24af9b67473bd5eaa0fed261b0711273dbd", null ],
      [ "COMMIT_TRANSACTION", "database__protocol_8h.html#ad8c43c9ed6f095c8422ee0db2e5adc24a81a165f193bfd4f8cfcea4a5e37188a6", null ],
      [ "ROLLBACK_TRANSACTION", "database__protocol_8h.html#ad8c43c9ed6f095c8422ee0db2e5adc24a0913aa38c75fa47ac5b2a88cafe91ede", null ],
      [ "TRANSACTION_RESPONSE", "database__protocol_8h.html#ad8c43c9ed6f095c8422ee0db2e5adc24a7dd442b2c672877f17692c2457cc2591", null ],
      [ "PREPARE_STATEMENT", "database__protocol_8h.html#ad8c43c9ed6f095c8422ee0db2e5adc24a58ac97d6ab7ccdf87e00f51b80be5c96", null ],
      [ "EXECUTE_PREPARED", "database__protocol_8h.html#ad8c43c9ed6f095c8422ee0db2e5adc24a4a409da7a17e56182e984ae5f61e7199", null ],
      [ "CLOSE_PREPARED", "database__protocol_8h.html#ad8c43c9ed6f095c8422ee0db2e5adc24ae8cc14cd4a6e19ad704174c160bfa413", null ],
      [ "ERROR_RESPONSE", "database__protocol_8h.html#ad8c43c9ed6f095c8422ee0db2e5adc24a029916a52dea691eac751d8eb1221b6f", null ]
    ] ],
    [ "query_operation", "database__protocol_8h.html#a9b0c80f74142419436e15e97b79ce3b4", [
      [ "SELECT", "database__protocol_8h.html#a9b0c80f74142419436e15e97b79ce3b4a63225f19fccb18e7c709f1fa11bc738e", null ],
      [ "INSERT", "database__protocol_8h.html#a9b0c80f74142419436e15e97b79ce3b4a61ee777e7f71dc466c3b2c685d8d313b", null ],
      [ "UPDATE", "database__protocol_8h.html#a9b0c80f74142419436e15e97b79ce3b4a15a8022d0ed9cd9c2a2e756822703eb4", null ],
      [ "DELETE", "database__protocol_8h.html#a9b0c80f74142419436e15e97b79ce3b4a32f68a60cef40faedbc6af20298c1a1e", null ],
      [ "CREATE", "database__protocol_8h.html#a9b0c80f74142419436e15e97b79ce3b4a294ce20cdefa29be3be0735cb62e715d", null ],
      [ "ALTER", "database__protocol_8h.html#a9b0c80f74142419436e15e97b79ce3b4ae709bb346ebf26dccb94b57ed09ba487", null ],
      [ "DROP", "database__protocol_8h.html#a9b0c80f74142419436e15e97b79ce3b4abf8f3be424eb6a72b21549fbb24ffb57", null ],
      [ "OTHER", "database__protocol_8h.html#a9b0c80f74142419436e15e97b79ce3b4a03570470bad94692ce93e32700d2e1cb", null ]
    ] ]
];