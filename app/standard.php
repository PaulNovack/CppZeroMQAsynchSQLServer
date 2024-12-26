<?php



$startTime = microtime(true);

// Database connection settings
$host = "mysql"; // Replace with your database host
$dbname = "testdb"; // Replace with your database name
$username = "paul"; // Replace with your database username
$password = "password"; // Replace with your database password

try {
    // Create a PDO connection
    $pdo = new PDO("mysql:host=$host;dbname=$dbname;charset=utf8mb4", $username, $password);
    $pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);

    echo "Connected to the database successfully.\n";

    $queries = [];
    $totalRows = 500000; // Adjust this to the total number of rows in the table

    for ($i = 0; $i < 25; $i++) {
        $randomOffset = mt_rand(0, $totalRows - 1);
        $query = "SELECT users.* FROM users LIMIT 100 OFFSET $randomOffset";
        $queries[] = $query;
        echo "Forming query ($i): $query\n";
    }

    $results = [];

    foreach ($queries as $index => $query) {
        
        $stmt = $pdo->query($query);
        $results[$index] = $stmt->fetchAll(PDO::FETCH_ASSOC);
        //var_dump($results[$index]);
        //echo "Query ($index + 1) executed successfully.\n";
    }

    // End time
    $endTime = microtime(true);

    echo "Ran: " . count($queries) . ' SQL queries' . PHP_EOL;

    // Calculate and display elapsed time
    $elapsedTime = ($endTime - $startTime) * 1000; // Convert seconds to milliseconds
    echo "Script executed in $elapsedTime milliseconds.\n";

} catch (PDOException $e) {
    // Handle database connection errors
    echo "Database connection failed: " . $e->getMessage() . "\n";
}

?>
