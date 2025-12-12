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
  MazeData: 12,
  RequestMaze: 13,
  ReachExit: 14,
  GameWin: 15,
  GameResult: 16,
  RestartRequest: 17,
  // NPC同步
  NpcActivate: 18,
  NpcUpdate: 19,
  NpcShoot: 20,
  NpcDamage: 21,
  // 玩家离开
  PlayerLeft: 22
};

// 房间管理
const rooms = new Map();

// 生成随机房间码（纯4位数字）
function generateRoomCode() {
  const code = Math.floor(1000 + Math.random() * 9000).toString();
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
        mazeData: null,  // 存储迷宫数据
        players: [{ socket: socket, reachedExit: false, isHost: true }],
        started: false
      };

      rooms.set(roomCode, room);
      socket.roomCode = roomCode;
      socket.isHost = true;

      console.log(`Room created: ${roomCode} (${mazeWidth}x${mazeHeight})`);

      // 发送房间创建成功
      const response = Buffer.alloc(2 + roomCode.length);
      response[0] = MessageType.RoomCreated;
      response[1] = roomCode.length;
      response.write(roomCode, 2);
      sendMessage(socket, response);
      break;
    }

    case MessageType.MazeData: {
      // 房主发送迷宫数据
      const roomCode = socket.roomCode;
      if (!roomCode) break;

      const room = rooms.get(roomCode);
      if (!room) break;

      // 只有房主可以发送迷宫数据
      if (!socket.isHost) break;

      // 存储迷宫数据
      room.mazeData = data;
      console.log(`Received maze data for room ${roomCode}, size: ${data.length} bytes`);

      // 如果有第二个玩家在等待，发送迷宫数据给他并开始游戏
      if (room.players.length === 2 && !room.started) {
        room.started = true;

        // 发送迷宫数据给第二个玩家
        const guest = room.players.find(p => !p.isHost);
        if (guest) {
          sendMessage(guest.socket, data);
          console.log(`Sent maze data to guest in room ${roomCode}`);
        }

        // 发送游戏开始消息给两个玩家
        const gameStartMsg = Buffer.alloc(1);
        gameStartMsg[0] = MessageType.GameStart;
        for (const player of room.players) {
          sendMessage(player.socket, gameStartMsg);
        }
        console.log(`Game started in room: ${roomCode}`);
      }
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

      room.players.push({ socket: socket, reachedExit: false, isHost: false });
      socket.roomCode = roomCode;
      socket.isHost = false;

      console.log(`Player joined room: ${roomCode}`);

      // 发送加入成功
      const response = Buffer.alloc(2 + roomCode.length);
      response[0] = MessageType.RoomJoined;
      response[1] = roomCode.length;
      response.write(roomCode, 2);
      sendMessage(socket, response);

      // 如果房主已经发送了迷宫数据，直接发送给新玩家并开始游戏
      if (room.mazeData) {
        room.started = true;

        // 发送迷宫数据给新玩家
        sendMessage(socket, room.mazeData);
        console.log(`Sent existing maze data to new player in room ${roomCode}`);

        // 发送游戏开始消息给两个玩家
        const gameStartMsg = Buffer.alloc(1);
        gameStartMsg[0] = MessageType.GameStart;
        for (const player of room.players) {
          sendMessage(player.socket, gameStartMsg);
        }
        console.log(`Game started in room: ${roomCode}`);
      } else {
        // 请求房主发送迷宫数据
        const host = room.players.find(p => p.isHost);
        if (host) {
          const requestMsg = Buffer.alloc(1);
          requestMsg[0] = MessageType.RequestMaze;
          sendMessage(host.socket, requestMsg);
          console.log(`Requested maze data from host in room ${roomCode}`);
        }
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
        console.log(`Player reached exit in room ${roomCode}`);

        // 检查是否所有玩家都到达终点
        const allReached = room.players.every(p => p.reachedExit);
        if (allReached) {
          // 发送游戏胜利消息
          const winMsg = Buffer.alloc(1);
          winMsg[0] = MessageType.GameWin;
          for (const p of room.players) {
            sendMessage(p.socket, winMsg);
          }
          console.log(`All players reached exit in room ${roomCode}! Game won!`);
        }
      }

      // 转发给其他玩家
      broadcastToRoom(room, socket, data);
      break;
    }

    case MessageType.GameResult: {
      const roomCode = socket.roomCode;
      if (!roomCode) break;

      const room = rooms.get(roomCode);
      if (!room || !room.started) break;

      // 转发游戏结果给其他玩家
      broadcastToRoom(room, socket, data);
      console.log(`Game result sent in room ${roomCode}`);
      break;
    }

    case MessageType.RestartRequest: {
      const roomCode = socket.roomCode;
      if (!roomCode) break;

      const room = rooms.get(roomCode);
      if (!room) break;

      // 重置房间状态，准备下一轮
      room.started = false;
      for (const player of room.players) {
        player.reachedExit = false;
      }

      // 转发重新开始请求给其他玩家
      broadcastToRoom(room, socket, data);
      console.log(`Restart request in room ${roomCode}`);
      break;
    }

    // NPC同步消息 - 直接转发给房间内其他玩家
    case MessageType.NpcActivate:
    case MessageType.NpcUpdate:
    case MessageType.NpcShoot:
    case MessageType.NpcDamage: {
      const roomCode = socket.roomCode;
      if (!roomCode) break;

      const room = rooms.get(roomCode);
      if (!room || !room.started) break;

      // 转发给其他玩家
      broadcastToRoom(room, socket, data);
      break;
    }
  }
}

// 创建服务器
const server = net.createServer((socket) => {
  console.log(`Client connected from ${socket.remoteAddress}`);

  let buffer = Buffer.alloc(0);

  socket.on('data', (data) => {
    buffer = Buffer.concat([buffer, data]);

    // 处理所有完整的消息
    while (buffer.length >= 2) {
      const msgLen = buffer.readUInt16LE(0);
      if (buffer.length < 2 + msgLen) break;

      const msgData = buffer.slice(2, 2 + msgLen);
      buffer = buffer.slice(2 + msgLen);

      try {
        handleMessage(socket, msgData);
      } catch (e) {
        console.error('Error handling message:', e);
      }
    }
  });

  socket.on('close', () => {
    console.log('Client disconnected');

    // 清理房间
    if (socket.roomCode) {
      const room = rooms.get(socket.roomCode);
      if (room) {
        const wasHost = socket.isHost;
        room.players = room.players.filter(p => p.socket !== socket);

        if (room.players.length === 0) {
          rooms.delete(socket.roomCode);
          console.log(`Room ${socket.roomCode} deleted`);
        } else {
          // 重置房间状态，让剩余玩家回到等待状态
          room.started = false;
          for (const player of room.players) {
            player.reachedExit = false;
          }

          // 如果离开的是房主，让剩余玩家成为新房主
          if (wasHost && room.players.length > 0) {
            room.players[0].isHost = true;
            room.players[0].socket.isHost = true;
            console.log(`New host assigned in room ${socket.roomCode}`);
          }

          // 通知剩余玩家对方已离开
          const playerLeftMsg = Buffer.alloc(1);
          playerLeftMsg[0] = MessageType.PlayerLeft;
          for (const player of room.players) {
            sendMessage(player.socket, playerLeftMsg);
          }
          console.log(`Notified remaining players in room ${socket.roomCode}`);
        }
      }
    }
  });

  socket.on('error', (err) => {
    console.error('Socket error:', err.message);
  });
});

const PORT = 9999;
server.listen(PORT, () => {
  console.log(`Tank Maze Server running on port ${PORT}`);
  console.log('Waiting for players...');
});
