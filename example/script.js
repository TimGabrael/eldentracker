const ctx = document.getElementById('heightChart').getContext('2d');
const map_canvas = document.getElementById('map');
const map_ctx = map_canvas.getContext('2d');
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
function drawCircle(ctx, x, y, radius, fill, stroke, strokeWidth) {
  ctx.beginPath()
  ctx.arc(x, y, radius, 0, 2 * Math.PI, false)
  if (fill) {
    ctx.fillStyle = fill
    ctx.fill()
  }
  if (stroke) {
    ctx.lineWidth = strokeWidth
    ctx.strokeStyle = stroke
    ctx.stroke()
  }
}


const socket = new WebSocket('ws://localhost:6252');

let x_index = 0;
let max_displayed = 10;
socket.onmessage = function(event) {
    const data = JSON.parse(event.data);
    if(data.damage !== undefined && data.damage.receiver !== undefined && data.damage.receiver.hp !== undefined && data.damage.receiver.is_player) {
        const new_hp = data.damage.receiver.hp;
        if(data.damage.receiver.normalized_map_pos !== undefined && data.damage.receiver.normalized_map_pos[0] !== undefined && data.damage.receiver.normalized_map_pos[1] !== undefined) {
            let pos_x = data.damage.receiver.normalized_map_pos[0] * map_canvas.width;
            let pos_y = data.damage.receiver.normalized_map_pos[1] * map_canvas.height;
            drawCircle(map_ctx, pos_x, pos_y, 4, 'white', 'red', 1);
        }

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
