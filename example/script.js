const ctx = document.getElementById('heightChart').getContext('2d');
const heightData = {
    labels: [],
    datasets: [{
        label: 'Height',
        data: [],
        borderColor: 'rgba(75, 192, 192, 1)',
        backgroundColor: 'rgba(75, 192, 192, 0.2)',
        borderWidth: 1,
        fill: false,
    }]
};

const heightChart = new Chart(ctx, {
    type: 'line',
    data: heightData,
    options: {
        responsive: true,
        scales: {
            x: {
                type: 'linear',
                position: 'bottom'
            },
            y: {
                beginAtZero: true
            }
        }
    }
});

// Open a WebSocket connection
const socket = new WebSocket('ws://localhost:6252');

let x_index = 0;
let max_displayed = 10;
socket.onmessage = function(event) {
    const data = JSON.parse(event.data);
    if(data.damage !== undefined && data.damage.receiver !== undefined && data.damage.receiver.hp !== undefined) {
        const new_hp = data.damage.receiver.hp;
        const currentTime = new Date().getTime();

        // Add the new height value to the chart data
        if(heightChart.data.datasets[0].data.length > max_displayed) {
            
            heightChart.data.datasets[0].data.shift();
        }
        heightChart.data.datasets[0].data.push({ x: x_index, y: new_hp });

        // Update the chart
        heightChart.update();
        x_index += 1;
    }
};

socket.onopen = function(event) {
    console.log("WebSocket is open now.");
    //socket.send("{\"hi\": \"whatup\"}");
};

socket.onclose = function(event) {
    console.log("WebSocket is closed now.");
};

socket.onerror = function(error) {
    console.error("WebSocket error:", error);
};
