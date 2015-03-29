// Do signals handling here.

process.on('SIGQUIT', function(a, b, c) {
    console.info('Got SIGQUIT, exiting.');
    process.exit();
});

process.on('SIGABRT', function(a, b, c) {
    console.info('Got SIGABRT, exiting.');
    process.exit();
});

process.on('SIGTERM', function(a, b, c) {
    console.info('Got SIGTERM, exiting.');
    process.exit();
});

process.on('SIGHUP', function(a, b, c) {
    console.info('Got SIGHUP, exiting.');
    process.exit();
});

process.on('SIGINT', function(a, b, c) {
    console.info('Got SIGINT, exiting.');
    process.exit();
});
