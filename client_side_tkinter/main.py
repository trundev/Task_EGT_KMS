import argparse
import socket
from datetime import timezone
import threading
import queue
import tkinter as tk
from tkinter import scrolledtext, messagebox
from protobuf import messages_pb2


SERVER_SOCKET_FAMILY = socket.AF_INET
SERVER_SOCKET_TYPE = socket.SOCK_STREAM
SERVER_PORT = 8080


#
# socket backend
#
class ServerConnection:
    socket: socket.socket

    def __init__(self, s):
        self.socket = s

    def send_protobuf(self, message: messages_pb2.PBMessage) -> None:
        """Serialize message using protobuf and send to server"""
        buffer = message.SerializeToString()
        len_bytes = len(buffer).to_bytes(4, byteorder='big')

        self.socket.sendall(len_bytes + buffer)

    def recv_protobuf(self) -> messages_pb2.PBMessage | None:
        """Receive buffer and parse to protobuf message"""
        size = self.socket.recv(4)
        if len(size) < 4:
            return None     # Connection was closed
        size = int.from_bytes(size, byteorder='big')
        buffer = b''
        while size > len(buffer):
            data = self.socket.recv(size - len(buffer))
            if len(data) == 0:
                return None     # Connection was closed
            buffer += data

        message = messages_pb2.PBMessage()
        res = message.ParseFromString(buffer)
        return message if res == size else None

    def send_login(self, username: str):
        """Send PBUserLogin message"""
        message = messages_pb2.PBMessage()
        message.login.user_name = username
        self.send_protobuf(message)

    def send_chat(self, text: str, *, sent_at=None, from_user: str|None=None):
        """Send PBChatMessage message"""
        message = messages_pb2.PBMessage()
        message.chat.text = text
        #message.chat.sent_at = sent_at
        if from_user is not None:
            message.chat.from_user = from_user
        self.send_protobuf(message)

    def send_command(self, text: str, parameter: str|None=None):
        """Send PBChatCommand message"""
        message = messages_pb2.PBMessage()
        message.command.command = text
        if parameter is not None:
            message.command.parameter = parameter
        self.send_protobuf(message)

# Thread-safe queue to pass data from socket to GUI
msg_queue = queue.Queue()

def socket_listener(server: ServerConnection):
    while True:
        try:
            message = server.recv_protobuf()
            if message is None:
                break
        except socket.error as ex:
            print('Exception:', ex)
            break
        msg_queue.put(message)
    msg_queue.shutdown()

#
# tkinter code
#
def tk_mainloop(title: str, server: ServerConnection) -> None:
    """Create main window"""
    root = tk.Tk()
    root.title(f"Chat - {title}")
    root.geometry("500x600")
    root.resizable(True, True)

    # Configure grid layout
    root.grid_rowconfigure(0, weight=1)  # Chat box row
    root.grid_rowconfigure(1, weight=0)  # Entry row
    root.grid_columnconfigure(0, weight=1)
    root.grid_columnconfigure(1, weight=0)

    # Chat display (scrollable)
    chat_display = scrolledtext.ScrolledText(root, wrap=tk.WORD, state='disabled')
    chat_display.grid(row=0, column=0, columnspan=2, sticky="nsew", padx=5, pady=5)

    # Entry box
    entry = tk.Entry(root)
    entry.grid(row=1, column=0, sticky="ew", padx=5, pady=5)

    # Send button box
    send_btn = tk.Button(root, text='Send')
    send_btn.grid(row=1, column=1, padx=5, pady=5)

    def chat_display_add(text: str) -> None:
        """Add text to chat dispay"""
        chat_display.config(state='normal')
        chat_display.insert(tk.END, text)
        chat_display.config(state='disabled')
        chat_display.see(tk.END)

    # Optional: Send message on Enter or via send_btn
    def send_message(event=None):
        message = entry.get()
        if message:
            # Chat text or server command
            if message.startswith('!'):
                server.send_command(*message[1:].split(maxsplit=1))
            else:
                chat_display_add(f"You: {message}\n")
                server.send_chat(message)
            entry.delete(0, tk.END)

    entry.bind("<Return>", send_message)
    send_btn.configure(command=send_message)

    def poll_queue():
        while not msg_queue.empty():
            msg: messages_pb2.PBMessage = msg_queue.get()
            active = msg.WhichOneof('payload')
            if active == 'chat':        # PBChatMessage
                sent_at = msg.chat.sent_at.ToDatetime().replace(tzinfo=timezone.utc)
                chat_display_add(
                        f'{sent_at.astimezone()} {msg.chat.from_user}\n'
                        f' {msg.chat.text}\n')
            elif active == 'result':    # PBCommandResult
                for text in msg.result.text:
                    chat_display_add(f'> {text}\n')
        # `is_shutdown` works after Python 3.13
        if getattr(msg_queue, 'is_shutdown', False):
            messagebox.showinfo("Goodbye", "Connection was closed, the application will now exit.")
            root.quit()
        else:
            root.after(100, poll_queue)

    poll_queue()
    root.mainloop()

def main(hostname: str, username: str):
    """Main entry point"""
    results = socket.getaddrinfo(hostname,
                                 port=SERVER_PORT,
                                 family=SERVER_SOCKET_FAMILY,
                                 type=SERVER_SOCKET_TYPE)
    family, socktype, proto, _, sockaddr = results[0]
    s = socket.socket(family, socktype, proto)
    s.connect(sockaddr)

    # Send login message
    server = ServerConnection(s)
    server.send_login(username)

    threading.Thread(target=socket_listener, args=(server,), daemon=True).start()

    tk_mainloop(hostname, server)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Chat client')
    parser.add_argument('hostname', help='Chat server socket address')
    parser.add_argument('username', help='Login user name')
    args = parser.parse_args()

    main(args.hostname, args.username)
