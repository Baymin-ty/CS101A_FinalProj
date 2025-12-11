const net = require('net');

// 消息类型（与 C++ 客户端一致）
const MessageType = {
  Connect: 1,
  ConnectAck: 2,
  Disconnect: 3,
  CreateRoom: 4,
  JoinRoom: 5,
  RoomCreated: 6,
  RoomJoined: 7,
  RoomError: 8,
  GameStart: 9,
  PlayerUpdate: 10,
  PlayerShoot: 11,
  ReachExit: 14,
  GameWin: 15
};

// 房间管理
const rooms = new Map();

// 生成随机房间码
function generateRoomCode() {
  const chars = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789';
  let code = '';
  for (let i = 0; i < 4; i++) {
    code += chars.charAt(Math.floor(Math.random() * chars.length));
  }
  return code;
}

// 发送消息给客户端
function sendMessage(socket, data) {
  const len = data.length;
  const packet = Buffer.alloc(2 + len);
  packet.writeUInt16LE(len, 0);
  data.copy(packet, 2);
  socket.write(packet);
}

// 广播给房间内其他玩家
function broadcastToRoom(room, senderSocket, data) {
  for (const player of room.players) {
    if (player.socket !== senderSocket) {
      sendMessage(player.socket, data);
    }
  }
}

// 处理消息
function handleMessage(socket, data) {
  if (data.length < 1) return;

  const msgType = data[0];

  switch (msgType) {
    case MessageType.Connect: {
      console.log('Client connected');
      // 发送连接确认
      const response = Buffer.alloc(1);
      response[0] = MessageType.ConnectAck;
      sendMessage(socket, response);
      break;
    }

    case MessageType.CreateRoom: {
      // 读取迷宫尺寸
      const mazeWidth = data.readUInt16LE(1);
      const mazeHeight = data.readUInt16LE(3);

      // 创建房间
      let roomCode;
      do {
        roomCode = generateRoomCode();
      } while (rooms.has(roomCode));

      const room = {
        code: roomCode,
        mazeWidth: mazeWidth,
        mazeHeight: mazeHeight,
        mazeSeed: Math.floor(Math.random() * 0xFFFFFFFF),
        players: [{ socket: socket, reachedExit: false }],
        started: false
      };

      rooms.set(roomCode, room);
      socket.roomCode = roomCode;

      console.log(`Room created: ${roomCode} (${mazeWidth}x${mazeHeight})`);

      // 发送房间创建成功
      const response = Buffer.alloc(2 + roomCode.length);
      response[0] = MessageType.RoomCreated;
      response[1] = roomCode.length;
      response.write(roomCode, 2);
      sendMessage(socket, response);
      break;
    }

    case MessageType.JoinRoom: {
      const codeLen = data[1];
      const roomCode = data.slice(2, 2 + codeLen).toString();

      const room = rooms.get(roomCode);
      if (!room) {
        // 房间不存在
        const error = "Room not found";
        const response = Buffer.alloc(2 + error.length);
        response[0] = MessageType.RoomError;
        response[1] = error.length;
        response.write(error, 2);
        sendMessage(socket, response);
        break;
      }

      if (room.players.length >= 2) {
        const error = "Room is full";
        const response = Buffer.alloc(2 + error.length);
        response[0] = MessageType.RoomError;
        response[1] = error.length;
        response.write(error, 2);
        sendMessage(socket, response);
        break;
      }

      room.players.push({ socket: socket, reachedExit: false });
      socket.roomCode = roomCode;

      console.log(`Player joined room: ${roomCode}`);

      // 发送加入成功
      const response = Buffer.alloc(2 + roomCode.length);
      response[0] = MessageType.RoomJoined;
      response[1] = roomCode.length;
      response.write(roomCode, 2);
      sendMessage(socket, response);

      // 如果房间满了，开始游戏
      if (room.players.length === 2) {
        room.started = true;

        // 发送游戏开始给所有玩家
        const gameStartMsg = Buffer.alloc(9);
        gameStartMsg[0] = MessageType.GameStart;
        gameStartMsg.writeUInt16LE(room.mazeWidth, 1);
        gameStartMsg.writeUInt16LE(room.mazeHeight, 3);
        gameStartMsg.writeUInt32LE(room.mazeSeed, 5);

        for (const player of room.players) {
          sendMessage(player.socket, gameStartMsg);
        }

        console.log(`Game started in room: ${roomCode}`);
      }
      break;
    }

    case MessageType.PlayerUpdate: {
      const roomCode = socket.roomCode;
      if (!roomCode) break;

      const room = rooms.get(roomCode);
      if (!room || !room.started) break;

      // 转发给其他玩家
      broadcastToRoom(room, socket, data);
      break;
    }

    case MessageType.PlayerShoot: {
      const roomCode = socket.roomCode;
      if (!roomCode) break;

      const room = rooms.get(roomCode);
      if (!room || !room.started) break;

      // 转发给其他玩家
      broadcastToRoom(room, socket, data);
      break;
    }

    case MessageType.ReachExit: {
      const roomCode = socket.roomCode;
      if (!roomCode) break;

      const room = rooms.get(roomCode);
      if (!room || !room.started) break;

      // 标记玩家到达终点
      const player = room.players.find(p => p.socket === socket);
      if (player) {
        player.reachedExit = true;
        console.log(`Player reached exit in room: ${roomCode}`);

        // 检查是否所有玩家都到达终点
        const allReached = room.players.every(p => p.reachedExit);
        if (allReached) {
          // 发送游戏胜利
          const winMsg = Buffer.alloc(1);
          winMsg[0] = MessageType.GameWin;

          for (const p of room.players) {
            sendMessage(p.socket, winMsg);
          }

          console.log(`Game won in room: ${roomCode}`);
        }
      }
      break;
    }

    case MessageType.Disconnect: {
      handleDisconnect(socket);
      break;
    }
  }
}

// 处理断开连接
function handleDisconnect(socket) {
  const roomCode = socket.roomCode;
  if (roomCode) {
    const room = rooms.get(roomCode);
    if (room) {
      room.players = room.players.filter(p => p.socket !== socket);
      if (room.players.length === 0) {
        rooms.delete(roomCode);
        console.log(`Room deleted: ${roomCode}`);
      } else {
        // 通知其他玩家
        const disconnectMsg = Buffer.alloc(1);
        disconnectMsg[0] = MessageType.Disconnect;
        for (const p of room.players) {
          sendMessage(p.socket, disconnectMsg);
        }
      }
    }
  }
}

// 创建服务器
const server = net.createServer((socket) => {
  console.log('Client connected from', socket.remoteAddress);

  let buffer = Buffer.alloc(0);

  socket.on('data', (chunk) => {
    buffer = Buffer.concat([buffer, chunk]);

    // 处理完整的消息
    while (buffer.length >= 2) {
      const len = buffer.readUInt16LE(0);

      if (buffer.length >= 2 + len) {
        const message = buffer.slice(2, 2 + len);
        buffer = buffer.slice(2 + len);
        handleMessage(socket, message);
      } else {
        break;
      }
    }
  });

  socket.on('close', () => {
    console.log('Client disconnected');
    handleDisconnect(socket);
  });

  socket.on('error', (err) => {
    console.log('Socket error:', err.message);
    handleDisconnect(socket);
  });
});

const PORT = 9999;
server.listen(PORT, '0.0.0.0', () => {
  console.log(`Tank Maze Server running on port ${PORT}`);
  console.log('Waiting for players...');
});
