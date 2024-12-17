#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

#define MAXLINE 1000
#define NAME_LEN 20

GtkWidget *text_view;
GtkTextBuffer *buffer;
GtkWidget *entry;
int sock;
char nickname[NAME_LEN];

int tcp_connect(char *server_ip, unsigned short port);
void errquit(const char *msg);
void append_message(const char *message);
void send_message(GtkWidget *widget, gpointer data);
void *receive_thread(void *arg);

int main(int argc, char *argv[]) {
    if (argc != 5) {
        printf("Usage: %s <server_ip> <port> <room_number> <nickname>\n", argv[0]);
        return 1;
    }

    gtk_init(&argc, &argv);

    strncpy(nickname, argv[4], NAME_LEN - 1);
    nickname[NAME_LEN - 1] = '\0';

    sock = tcp_connect(argv[1], atoi(argv[2]));
    if (sock == -1) errquit("tcp_connect failed");

    char init_msg[MAXLINE];
    sprintf(init_msg, "%s %s", argv[3], nickname);
    send(sock, init_msg, strlen(init_msg), 0);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Chat Client");
    gtk_window_set_default_size(GTK_WINDOW(window), 400, 300);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    text_view = gtk_text_view_new();
    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scroll), text_view);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    entry = gtk_entry_new();
    GtkWidget *send_button = gtk_button_new_with_label("Send");
    gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), send_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    g_signal_connect(send_button, "clicked", G_CALLBACK(send_message), NULL);
    g_signal_connect(entry, "activate", G_CALLBACK(send_message), NULL);

    pthread_t tid;
    pthread_create(&tid, NULL, receive_thread, NULL);

    gtk_widget_show_all(window);
    gtk_main();

    close(sock);
    return 0;
}

int tcp_connect(char *server_ip, unsigned short port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;

    struct sockaddr_in serv_addr = {0};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(server_ip);
    serv_addr.sin_port = htons(port);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        return -1;

    return sock;
}

void errquit(const char *msg) {
    perror(msg);
    exit(1);
}

void append_message(const char *message) {
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(buffer, &end);
    gtk_text_buffer_insert(buffer, &end, message, -1);
    gtk_text_buffer_insert(buffer, &end, "\n", 1);
}

void send_message(GtkWidget *widget, gpointer data) {
    const char *message = gtk_entry_get_text(GTK_ENTRY(entry));
    if (strlen(message) > 0) {
        char bufall[MAXLINE];
        sprintf(bufall, "%s: %s", nickname, message);
        if (send(sock, bufall, strlen(bufall), 0) < 0) {
            perror("send failed");
            return;
        }
        gtk_entry_set_text(GTK_ENTRY(entry), "");
    }
}

void *receive_thread(void *arg) {
    char buffer[MAXLINE];
    int n;
    while ((n = recv(sock, buffer, MAXLINE, 0)) > 0) {
        buffer[n] = 0;
        gdk_threads_add_idle((GSourceFunc)append_message, strdup(buffer));
    }
    return NULL;
}
