// Do signals handling here.

process.on('SIGQUIT', function() {
    console.info('Got SIGQUIT, exiting.');
    process.exit();
});

process.on('SIGABRT', function() {
    console.info('Got SIGABRT, exiting.');
    process.exit();
});

process.on('SIGTERM', function() {
    console.info('Got SIGTERM, exiting.');
    process.exit();
});

process.on('SIGHUP', function() {
    console.info('Got SIGHUP, exiting.');
    process.exit();
});

process.on('SIGINT', function() {
    console.info('Got SIGINT, exiting.');
    process.exit();
});
