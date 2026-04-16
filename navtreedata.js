/*
 @licstart  The following is the entire license notice for the JavaScript code in this file.

 The MIT License (MIT)

 Copyright (C) 1997-2020 by Dimitri van Heesch

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 and associated documentation files (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge, publish, distribute,
 sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or
 substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 @licend  The above is the entire license notice for the JavaScript code in this file
*/
var NAVTREE =
[
  [ "Database System", "index.html", [
    [ "System Overview", "index.html#overview", null ],
    [ "Key Features", "index.html#features", null ],
    [ "Architecture Diagram", "index.html#architecture", null ],
    [ "Quick Start", "index.html#quickstart", null ],
    [ "Installation", "index.html#installation", [
      [ "CMake FetchContent (Recommended)", "index.html#install_fetchcontent", null ],
      [ "vcpkg", "index.html#install_vcpkg", null ],
      [ "Manual Build", "index.html#install_manual", null ]
    ] ],
    [ "Module Overview", "index.html#modules", null ],
    [ "Learning Resources", "index.html#learning", null ],
    [ "Examples", "index.html#examples", null ],
    [ "Related Systems", "index.html#related", null ],
    [ "Database System Samples", "md_samples_2README.html", [
      [ "Sample Programs Overview", "md_samples_2README.html#autotoc_md12", [
        [ "Core Database Features", "md_samples_2README.html#autotoc_md13", [
          [ "basic_usage_sample.cpp", "md_samples_2README.html#autotoc_md14", null ],
          [ "postgresql_advanced_sample.cpp", "md_samples_2README.html#autotoc_md15", null ],
          [ "connection_pool_demo.cpp", "md_samples_2README.html#autotoc_md16", null ]
        ] ],
        [ "Phase 4: Advanced Enterprise Features", "md_samples_2README.html#autotoc_md17", [
          [ "orm_framework_demo.cpp", "md_samples_2README.html#autotoc_md18", null ],
          [ "performance_monitoring_demo.cpp", "md_samples_2README.html#autotoc_md19", null ],
          [ "security_framework_demo.cpp", "md_samples_2README.html#autotoc_md20", null ],
          [ "async_operations_demo.cpp", "md_samples_2README.html#autotoc_md21", null ]
        ] ],
        [ "Query Builder Demonstrations", "md_samples_2README.html#autotoc_md22", [
          [ "sql_query_builder_examples.cpp", "md_samples_2README.html#autotoc_md23", null ],
          [ "mongodb_query_builder_examples.cpp", "md_samples_2README.html#autotoc_md24", null ],
          [ "redis_query_builder_examples.cpp", "md_samples_2README.html#autotoc_md25", null ]
        ] ],
        [ "Multi-Database Examples", "md_samples_2README.html#autotoc_md26", [
          [ "multi_database_examples.cpp", "md_samples_2README.html#autotoc_md27", null ]
        ] ]
      ] ],
      [ "Building the Samples", "md_samples_2README.html#autotoc_md28", [
        [ "Prerequisites", "md_samples_2README.html#autotoc_md29", null ],
        [ "Build Instructions", "md_samples_2README.html#autotoc_md30", null ],
        [ "Alternative Build (samples only)", "md_samples_2README.html#autotoc_md31", null ]
      ] ],
      [ "Database Setup", "md_samples_2README.html#autotoc_md32", [
        [ "PostgreSQL Setup for Samples", "md_samples_2README.html#autotoc_md33", null ],
        [ "Connection Configuration", "md_samples_2README.html#autotoc_md34", null ],
        [ "Running Without Database", "md_samples_2README.html#autotoc_md35", null ]
      ] ],
      [ "Sample Output Examples", "md_samples_2README.html#autotoc_md36", [
        [ "Basic Usage Output", "md_samples_2README.html#autotoc_md37", null ],
        [ "PostgreSQL Advanced Features Output", "md_samples_2README.html#autotoc_md38", null ],
        [ "Connection Pool Demo Output", "md_samples_2README.html#autotoc_md39", null ]
      ] ],
      [ "Understanding the Results", "md_samples_2README.html#autotoc_md40", [
        [ "Performance Metrics", "md_samples_2README.html#autotoc_md41", null ],
        [ "PostgreSQL Features", "md_samples_2README.html#autotoc_md42", null ],
        [ "Connection Management", "md_samples_2README.html#autotoc_md43", null ]
      ] ],
      [ "Advanced Usage", "md_samples_2README.html#autotoc_md44", [
        [ "Customizing Database Configuration", "md_samples_2README.html#autotoc_md45", null ],
        [ "Performance Tuning", "md_samples_2README.html#autotoc_md46", null ],
        [ "Adding New Samples", "md_samples_2README.html#autotoc_md47", null ]
      ] ],
      [ "Troubleshooting", "md_samples_2README.html#autotoc_md48", [
        [ "Common Issues", "md_samples_2README.html#autotoc_md49", null ],
        [ "Performance Considerations", "md_samples_2README.html#autotoc_md50", null ],
        [ "Getting Help", "md_samples_2README.html#autotoc_md51", null ]
      ] ],
      [ "PostgreSQL Feature Reference", "md_samples_2README.html#autotoc_md52", [
        [ "Supported PostgreSQL Features", "md_samples_2README.html#autotoc_md53", null ],
        [ "Sample Query Examples", "md_samples_2README.html#autotoc_md54", null ]
      ] ],
      [ "License", "md_samples_2README.html#autotoc_md55", null ]
    ] ],
    [ "database_system Examples", "md_examples_2README.html", [
      [ "Examples", "md_examples_2README.html#autotoc_md57", null ],
      [ "Building", "md_examples_2README.html#autotoc_md58", null ],
      [ "Running Without a Database", "md_examples_2README.html#autotoc_md59", null ],
      [ "Running With PostgreSQL", "md_examples_2README.html#autotoc_md60", null ],
      [ "Relationship to samples/", "md_examples_2README.html#autotoc_md61", null ]
    ] ],
    [ "README", "md_README.html", [
      [ "Database System", "md_README.html#autotoc_md62", [
        [ "Table of Contents", "md_README.html#autotoc_md63", null ],
        [ "Overview", "md_README.html#autotoc_md65", [
          [ "v1.0.0 Release (2026-04)", "md_README.html#autotoc_md66", null ],
          [ "Previous Updates (2026-01)", "md_README.html#autotoc_md67", null ],
          [ "Previous Updates (2025-12)", "md_README.html#autotoc_md68", null ]
        ] ],
        [ "Requirements", "md_README.html#autotoc_md70", [
          [ "Database Backends (at least one required)", "md_README.html#autotoc_md71", null ],
          [ "Dependency Flow", "md_README.html#autotoc_md72", null ],
          [ "Building with Dependencies", "md_README.html#autotoc_md73", null ],
          [ "C++20 Module Support", "md_README.html#autotoc_md74", null ]
        ] ],
        [ "Core Features", "md_README.html#autotoc_md76", [
          [ "Multi-Backend Support", "md_README.html#autotoc_md77", null ],
          [ "Experimental Features", "md_README.html#autotoc_md78", null ],
          [ "Quick Start — Database Connection", "md_README.html#autotoc_md79", null ],
          [ "Type-Safe Query Builders", "md_README.html#autotoc_md80", null ],
          [ "ORM Framework (C++20 Concepts-based)", "md_README.html#autotoc_md81", null ],
          [ "Result Types", "md_README.html#autotoc_md82", null ]
        ] ],
        [ "Performance Highlights", "md_README.html#autotoc_md84", [
          [ "Benchmarks (Intel i7-9750H @ 2.6GHz, 16GB RAM, SSD)", "md_README.html#autotoc_md85", null ]
        ] ],
        [ "Quick Start", "md_README.html#autotoc_md87", [
          [ "Installation via vcpkg", "md_README.html#autotoc_md88", null ],
          [ "Prerequisites", "md_README.html#autotoc_md89", null ],
          [ "Installation", "md_README.html#autotoc_md90", null ],
          [ "Basic Usage", "md_README.html#autotoc_md91", null ],
          [ "Unified Database System (Zero-Config)", "md_README.html#autotoc_md92", null ]
        ] ],
        [ "Architecture Overview", "md_README.html#autotoc_md94", null ],
        [ "Ecosystem Integration", "md_README.html#autotoc_md96", [
          [ "Ecosystem Dependency Map", "md_README.html#autotoc_md97", null ],
          [ "Project Dependencies", "md_README.html#autotoc_md98", null ]
        ] ],
        [ "Documentation", "md_README.html#autotoc_md100", [
          [ "Getting Started", "md_README.html#autotoc_md101", null ],
          [ "Core Documentation", "md_README.html#autotoc_md102", null ],
          [ "Advanced Topics", "md_README.html#autotoc_md103", null ],
          [ "Development", "md_README.html#autotoc_md104", null ]
        ] ],
        [ "CMake Integration", "md_README.html#autotoc_md106", [
          [ "As a Subdirectory", "md_README.html#autotoc_md107", null ],
          [ "With FetchContent", "md_README.html#autotoc_md108", null ],
          [ "Build Options", "md_README.html#autotoc_md109", null ]
        ] ],
        [ "Production Quality", "md_README.html#autotoc_md111", [
          [ "Build & Testing Infrastructure", "md_README.html#autotoc_md112", null ],
          [ "Thread Safety & Concurrency", "md_README.html#autotoc_md113", null ],
          [ "Resource Management (RAII)", "md_README.html#autotoc_md114", null ],
          [ "Error Handling", "md_README.html#autotoc_md115", null ]
        ] ],
        [ "Performance Baselines", "md_README.html#autotoc_md117", [
          [ "Key Metrics", "md_README.html#autotoc_md118", null ]
        ] ],
        [ "", "md_README.html#autotoc_md119", null ],
        [ "Contributing", "md_README.html#autotoc_md120", null ],
        [ "License", "md_README.html#autotoc_md122", null ],
        [ "Support & Community", "md_README.html#autotoc_md124", null ],
        [ "Acknowledgments", "md_README.html#autotoc_md126", null ]
      ] ]
    ] ],
    [ "Quick Start Tutorial", "tutorial_quickstart.html", [
      [ "Prerequisites", "tutorial_quickstart.html#qs_prereq", null ],
      [ "Linking the Library", "tutorial_quickstart.html#qs_link", null ],
      [ "Step 1: Connection Setup", "tutorial_quickstart.html#qs_connect", null ],
      [ "Step 2: Basic CRUD", "tutorial_quickstart.html#qs_crud", null ],
      [ "Step 3: Result Handling", "tutorial_quickstart.html#qs_results", null ],
      [ "Step 4: Transaction Management", "tutorial_quickstart.html#qs_tx", null ],
      [ "Next Steps", "tutorial_quickstart.html#qs_next", null ]
    ] ],
    [ "ORM Entity Mapping Tutorial", "tutorial_orm.html", [
      [ "Core Concepts", "tutorial_orm.html#orm_concepts", null ],
      [ "Step 1: Define an Entity", "tutorial_orm.html#orm_define", null ],
      [ "Step 2: Constraints", "tutorial_orm.html#orm_constraints", null ],
      [ "Step 3: Relationships", "tutorial_orm.html#orm_relationships", null ],
      [ "Step 4: Query Builder Integration", "tutorial_orm.html#orm_querybuilder", null ],
      [ "Inspecting Metadata at Runtime", "tutorial_orm.html#orm_inspect", null ],
      [ "Next Steps", "tutorial_orm.html#orm_next", null ]
    ] ],
    [ "Multi-Backend Tutorial", "tutorial_backends.html", [
      [ "Backend Capability Matrix", "tutorial_backends.html#be_matrix", null ],
      [ "Selecting a Backend", "tutorial_backends.html#be_select", null ],
      [ "Backend-Specific Features", "tutorial_backends.html#be_features", [
        [ "SQLite", "tutorial_backends.html#be_sqlite", null ],
        [ "PostgreSQL", "tutorial_backends.html#be_pg", null ],
        [ "MySQL", "tutorial_backends.html#be_mysql", null ],
        [ "MongoDB", "tutorial_backends.html#be_mongo", null ]
      ] ],
      [ "Connection Modes: Direct vs Proxy", "tutorial_backends.html#be_modes", null ],
      [ "Migrating Between Backends", "tutorial_backends.html#be_migration", null ],
      [ "Next Steps", "tutorial_backends.html#be_next", null ]
    ] ],
    [ "Frequently Asked Questions", "faq.html", [
      [ "Which backend should I choose?", "faq.html#faq_backend_choice", null ],
      [ "How do I configure connection pooling?", "faq.html#faq_pooling", null ],
      [ "What about transaction isolation levels?", "faq.html#faq_isolation", null ],
      [ "How do I handle schema migrations?", "faq.html#faq_migrations", null ],
      [ "Query builder vs raw SQL — when should I use which?", "faq.html#faq_querybuilder", null ],
      [ "Does the ORM hurt performance?", "faq.html#faq_orm_perf", null ],
      [ "Does database_system support prepared statements?", "faq.html#faq_prepared", null ],
      [ "How do async operations work?", "faq.html#faq_async", null ],
      [ "How should I handle errors and recovery?", "faq.html#faq_recovery", null ],
      [ "How do I implement multi-tenancy?", "faq.html#faq_multitenant", null ],
      [ "More questions?", "faq.html#faq_more", null ]
    ] ],
    [ "Troubleshooting Guide", "troubleshooting.html", [
      [ "Connection Failures", "troubleshooting.html#ts_connect", [
        [ "Symptom: connect_result returns \"connection refused\"", "troubleshooting.html#ts_connect_refused", null ],
        [ "Symptom: connection hangs forever", "troubleshooting.html#ts_connect_timeout", null ]
      ] ],
      [ "Transaction Deadlocks and Locking Errors", "troubleshooting.html#ts_transactions", [
        [ "Symptom: \"deadlock detected\" or \"Lock wait timeout exceeded\"", "troubleshooting.html#ts_tx_deadlock", null ],
        [ "SQLite-specific: SQLITE_BUSY / \"database is locked\"", "troubleshooting.html#ts_tx_busy", null ]
      ] ],
      [ "ORM Entity Mapping Errors", "troubleshooting.html#ts_orm", [
        [ "Symptom: empty fields() or missing columns in CREATE TABLE", "troubleshooting.html#ts_orm_metadata", null ],
        [ "Symptom: \"NOT NULL constraint failed\" on insert", "troubleshooting.html#ts_orm_constraint", null ]
      ] ],
      [ "Query Performance Issues", "troubleshooting.html#ts_perf", [
        [ "Symptom: a SELECT that \"should be fast\" is slow", "troubleshooting.html#ts_perf_slow", null ],
        [ "Symptom: throughput drops as concurrency rises", "troubleshooting.html#ts_perf_pool", null ]
      ] ],
      [ "Migration Conflicts", "troubleshooting.html#ts_migrate", [
        [ "Symptom: \"column already exists\" or \"relation does not exist\"", "troubleshooting.html#ts_migrate_drift", null ],
        [ "Symptom: works on PostgreSQL, fails on MySQL or SQLite", "troubleshooting.html#ts_migrate_dialect", null ]
      ] ],
      [ "Still stuck?", "troubleshooting.html#ts_more", null ]
    ] ],
    [ "Test List", "test.html", null ],
    [ "Namespaces", "namespaces.html", [
      [ "Namespace List", "namespaces.html", "namespaces_dup" ],
      [ "Namespace Members", "namespacemembers.html", [
        [ "All", "namespacemembers.html", null ],
        [ "Functions", "namespacemembers_func.html", null ],
        [ "Variables", "namespacemembers_vars.html", null ],
        [ "Typedefs", "namespacemembers_type.html", null ],
        [ "Enumerations", "namespacemembers_enum.html", null ]
      ] ]
    ] ],
    [ "Concepts", "concepts.html", "concepts" ],
    [ "Classes", "annotated.html", [
      [ "Class List", "annotated.html", "annotated_dup" ],
      [ "Class Index", "classes.html", null ],
      [ "Class Hierarchy", "hierarchy.html", "hierarchy" ],
      [ "Class Members", "functions.html", [
        [ "All", "functions.html", "functions_dup" ],
        [ "Functions", "functions_func.html", "functions_func" ],
        [ "Variables", "functions_vars.html", "functions_vars" ],
        [ "Typedefs", "functions_type.html", null ],
        [ "Enumerations", "functions_enum.html", null ],
        [ "Related Symbols", "functions_rela.html", null ]
      ] ]
    ] ],
    [ "Files", "files.html", [
      [ "File List", "files.html", "files_dup" ],
      [ "File Members", "globals.html", [
        [ "All", "globals.html", "globals_dup" ],
        [ "Functions", "globals_func.html", "globals_func" ],
        [ "Variables", "globals_vars.html", null ],
        [ "Macros", "globals_defs.html", null ]
      ] ]
    ] ],
    [ "Examples", "examples.html", "examples" ]
  ] ]
];

var NAVTREEINDEX =
[
"_2home_2runner_2work_2database_system_2database_system_2database_2async_2async_operations_8h-example.html",
"classBenchmarkUser.html#abeaf0308ffcc792ff698da1c23249232",
"classdatabase_1_1async_1_1async__executor.html#afb6da7ec2dbb74622874d6e6a916a98a",
"classdatabase_1_1backends_1_1postgresql__backend.html#a1825a44bedbbcc66f993457967300043",
"classdatabase_1_1database__context.html#a2a350b462625ec049954d0191017417f",
"classdatabase_1_1integrated_1_1adapters_1_1backends_1_1common__logger__backend.html#afb6987b4989ba85a2a6a918b2762be7e",
"classdatabase_1_1integrated_1_1adapters_1_1backends_1_1null__monitoring__backend.html#ae9765fe704919be43b548bf6bf4fa1ca",
"classdatabase_1_1integrated_1_1adapters_1_1thread__adapter.html#a702007d7fdf5920d9a674018af1ca1b9",
"classdatabase_1_1integrated_1_1unified__database__system_1_1builder.html#a102ff9a5f694a3c331e63ae4321cff68",
"classdatabase_1_1monitoring_1_1query__timer.html#aeae9b14c39174f181f57ee56f6e30f76",
"classdatabase_1_1protocol_1_1protocol__serializer.html#ada75a8d43570f2bef4d5571e9472a3e1",
"classdatabase_1_1query__builder.html#ac4c1d89cc71f8eaab3ea9abec33f9136",
"classdatabase_1_1security_1_1encryption__manager.html#a25a9a11b2aefd1a6986adc832bb44037",
"classdatabase_1_1testing_1_1expectation__builder.html#a6eb0264cfe5eb55c87a5fcd69323712d",
"classdatabase_1_1testing_1_1mock__database.html#abacda3d530defe28325890f8778de51e",
"credential__test_8cpp.html#a299a1ead244428722cf02fd3ce09a363",
"functions_u.html",
"mock__expectations_8h.html#a20833ceb649ffa3e2b520f6573f630ee",
"namespacedatabase_1_1query_1_1tests.html#ae5b54d05f04df9e5b91641f8f4b03819",
"secure__connection_8h.html#ac5e46808f7c92f16aadf5a24631e5a60",
"structdatabase_1_1integrated_1_1database__config.html#a21d794b55c81724f661c803a4cf2a47e",
"structdatabase_1_1integrated_1_1unified__db__config.html#a8699e196a4725378b148ca98d8311c09",
"structdatabase_1_1query_1_1join__spec.html",
"test__connection__string__builder_8cpp.html#a4defa508854b29d56bfda3badb83c164",
"test__protocol__serializer_8cpp.html#af9884507f832517739901361a4233fce"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';