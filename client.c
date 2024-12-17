#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>

#define MAXLINE 1000
#define NAME_LEN 20
#define FILE_CHUNK_SIZE 1024

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
void send_file(GtkWidget *widget, gpointer data);
void set_text_view_font(GtkWidget *text_view);


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
    set_text_view_font(text_view);
    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scroll), text_view);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    entry = gtk_entry_new();
    GtkWidget *send_button = gtk_button_new_with_label("Send");
    GtkWidget *file_button = gtk_button_new_with_label("Send File");  // 파일 전송 버튼 추가
    gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), send_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), file_button, FALSE, FALSE, 0); // 버튼 박스에 추가
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    g_signal_connect(send_button, "clicked", G_CALLBACK(send_message), NULL);
    g_signal_connect(entry, "activate", G_CALLBACK(send_message), NULL);
    g_signal_connect(file_button, "clicked", G_CALLBACK(send_file), NULL); // 파일 전송 함수 연결

    pthread_t tid;
    pthread_create(&tid, NULL, receive_thread, NULL);

    gtk_widget_show_all(window);
    gtk_main();

    close(sock);
    return 0;
}

void send_file(GtkWidget *widget, gpointer data) {
    GtkWidget *dialog;
    dialog = gtk_file_chooser_dialog_new("Select File",
                                        NULL,
                                        GTK_FILE_CHOOSER_ACTION_OPEN,
                                        "_Cancel", GTK_RESPONSE_CANCEL,
                                        "_Open", GTK_RESPONSE_ACCEPT,
                                        NULL);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        FILE *file = fopen(filename, "rb");

        if (file == NULL) {
            perror("File open error");
            g_free(filename);
            gtk_widget_destroy(dialog);
            return;
        }

        // 서버에 파일 전송 시작 알림
        char header[MAXLINE];
        snprintf(header, MAXLINE, "%s: FILE [%s]\n", nickname, strrchr(filename, '/') ? strrchr(filename, '/') + 1 : filename);
        send(sock, header, strlen(header), 0);

        // 파일 내용을 소켓을 통해 전송
        char file_buffer[FILE_CHUNK_SIZE];
        size_t bytes_read;
        while ((bytes_read = fread(file_buffer, 1, FILE_CHUNK_SIZE, file)) > 0) {
            send(sock, file_buffer, bytes_read, 0);
        }

        // 전송 완료 메시지
        //const char *done_msg = "FILE__DONE\n";
        //send(sock, done_msg, strlen(done_msg), 0);

        append_message("File sent successfully.\n");

        fclose(file);        

        g_free(filename);
    }


    gtk_widget_destroy(dialog);
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
        // 메시지 처리: FILE_TRANSFER_DONE 메시지가 오면 파일 이름만 표시하도록 처리
        if (strstr(buffer, "FILE_TRANSFER_DONE") != NULL) {
            append_message("File transfer complete.");
        } else {
            // 일반 메시지 처리
            gdk_threads_add_idle((GSourceFunc)append_message, strdup(buffer));
        }
    }
    return NULL;
}

void set_text_view_font(GtkWidget *text_view) {
    GtkCssProvider *provider = gtk_css_provider_new();
    GdkDisplay *display = gdk_display_get_default();
    GdkScreen *screen = gdk_display_get_default_screen(display);

    // CSS 스타일을 적용
    gtk_css_provider_load_from_data(provider,
        "textview { font-family: 'NanumGothic'; font-size: 12pt; }", -1, NULL);

    gtk_style_context_add_provider_for_screen(screen,
        GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_USER);

    g_object_unref(provider);
}