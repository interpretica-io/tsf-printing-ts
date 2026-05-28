# Copyright (C) 2026 Interpretica Unipessoal Lda

tests = [
    'prologue',
    'epilogue',
    'detect',
    'ipp',
    'cups_queue',
    'cups_jobs',
    'audit',
]

foreach test : tests
    test_exe = test
    test_c = test + '.c'
    package_tests_c += [ test_c ]
    executable(test_exe, test_c, install: true, install_dir: package_dir,
               dependencies: test_deps)
endforeach

install_data(['package.xml'], install_dir: package_dir)
