<?php

$startTime = microtime(true);


$context = new ZMQContext();
$socket = $context->getSocket(ZMQ::SOCKET_DEALER);

$clientId = uniqid("client_");
$socket->setSockOpt(ZMQ::SOCKOPT_IDENTITY, $clientId);
$socket->connect("tcp://zeromqasyncmysql:5555");

echo "Client ID: $clientId\n";

$queries = [];

for ($i = 0; $i < 100; $i++) {
    $totalRows = 500000; // Adjust this to the total number of rows in the table
    $randomOffset = mt_rand(0, $totalRows - 1);
    $query = "SELECT person.* FROM person LIMIT 100 OFFSET $randomOffset";
    $queries[] = $query;
}

$queryMap = [];
foreach ($queries as $query) {
    $queryId = uniqid("query_");
    $queryMap[$queryId] = $query;

    // Serialize the request using MessagePack
    $payload = msgpack_pack(['id' => $queryId, 'query' => $query]);
    $socket->sendmulti(['', $payload]);

    echo "Sent query ($queryId): $query\n";
}

$receivedResponses = [];
while (count($receivedResponses) < count($queries)) {
    $response = $socket->recvMulti();
    $payload = msgpack_unpack($response[0]);

    if (isset($payload['id']) && isset($queryMap[$payload['id']])) {
        $queryId = $payload['id'];
        $receivedResponses[$queryId] = $payload['data'];
        echo  "\n</ br></ br>";
        print_r($payload['data']);
    } else {
        echo "Received response with unknown query ID.\n";
    }
}

// End time
$endTime = microtime(true);

echo "Ran: " . sizeof($queries) . ' SQL queries' . PHP_EOL;
// Calculate and display elapsed time
$elapsedTime = ($endTime - $startTime) * 1000; // Convert seconds to milliseconds
echo "Script executed in $elapsedTime milliseconds.\n";

