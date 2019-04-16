/*
複数のクライアント同士でのチャットを処理するサーバのプログラム。
通信要求がある場合、新しいソケットを生成する。クライアント数が上限に達した場合、警告文をサーバとクライアントに表示し、新しいソケットが生成されないようにする。
新しくログインしたクライアントにはログインしているクライアントの名前を送信する。
クライアントが新しく接続された時、クライアントがログアウトした時に他のクライアントに名前と共にログイン/ログアウトの旨を送信する。クライアントから"LOGOUT"メッセージを受け取ったときに、ログアウト状態とみなし他のクライアントが入れるようにしている。
通常のメッセージが送られてきた場合は、全クライアントにメッセージを送信する。
"SERVER_END"と標準入力すると、全クライアントにソケットを閉じるように促し、全クライアントのソケットが閉じられたのを確認してから、ソケットを閉じてプログラムを終了する。
Ctrl + Cによる強制終了などでサーバのソケットが先に閉じられた場合にも、クライアントのソケットを閉じるようなプログラムになってはいるが、本チャットソフトでは推奨されていない。
*/
#include<stdio.h>
#include<arpa/inet.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<string.h>
#include<stdlib.h>
#include<poll.h>

#define MAX_USER 10 		//接続するユーザ(クライアント)の上限を設定

//ユーザ(クライアント)の情報を持つ構造体
struct user{
	char name[128];		//クライアントの名前
	int login;			//1でログイン状態,0でログアウト状態
	struct pollfd new_sockfd[1];	//pollで見る読み込みソケット
};


/**
*エラー発生時に強制終了させる関数
*引数: どこで起きたエラーかを示すmessage
*返値: なし(この関数が呼ばれた時プログラムを終了する)
*/
void myerror(char *message){
	perror(message);
	exit(1);
}

/**
*サーバの操作："SERVER_END"とキーボード入力するとサーバを終了する
*/
int main(){
	int sockfd;			//受付用ソケット
	struct user user[MAX_USER];		//クライアントを管理する配列
	char buff[128], new_buff[128];	//送受信データ
	struct pollfd fds[2];			//poll()で見るための読み込みソケット
	struct sockaddr_in serv_addr;	//サーバデータ
	int chkerr, i, j, k, nuser = 0;	//chkerr：システムコールの返り値を保持する変数
				//i,j,k：カウンタ変数 //nuser：ログイン状態のクライアントの人数
	//int flag = 1; //デバッグ次時有効

	//全クライアント用のログイン状態を初期化
	for(i=0;i<MAX_USER;i++){
		user[i].login = 0;
	}

	//ソケットを作る
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd < 0) myerror("socet_error");	//エラーチェック,エラー時myerrorを呼ぶ

	//アドレスを作る
	memset(&serv_addr, 0, sizeof(struct sockaddr_in));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(10000);

	//setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(int));		//デバッグ時有効

	//ソケットにアドレスを割り当てる
	chkerr = bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr_in));
	if(chkerr < 0) myerror("bind_error");

	fds[0].fd = sockfd;			//poll()で見る読み込みソケットとしてsockfdを登録
	fds[0].events = POLLIN;		//reventsにPOLLINが返ってくるように設定する

	fds[1].fd = 0;				//poll()で見る読み込みソケットとして標準入力を登録
	fds[1].events = POLLIN;		//reventsにPOLLINが返ってくるように設定する

	chkerr = listen(fds[0].fd, 5); //コネクション要求を待ち始めるよう指示
	if(chkerr < 0) myerror("listen_error");

	printf("このサーバーの最大接続人数は%dです\n",MAX_USER);
	while(1){
		poll(fds,2,0);	//sockfdと標準入力を監視
		if(fds[0].revents == POLLIN  &&  nuser < MAX_USER){
		//sockfdに通信要求がある場合入る。クライアントの人数が上限に達している場合は入らない
			for(j=0; j< MAX_USER; j++){
				if(user[j].login != 1){	//ログインされていないuser[j]にクライアントを登録
					user[j].new_sockfd[0].fd = accept(fds[0].fd, NULL, NULL);	//fdを配列に保存
					if(user[j].new_sockfd[0].fd == -1) myerror("accept_error");

					user[j].new_sockfd[0].events = POLLIN;		//reventsにPOLLINが返ってくるように設定する

					chkerr = read(user[j].new_sockfd[0].fd, buff, 128);	//クライアントからデータ(クライアントの名前)を受け取る
					if(chkerr < 0) myerror("read_name_error");

					strcpy(user[j].name, buff);	//nameに名前を保存する
					user[j].login = 1;			//ログイン状態にする
					nuser++;				//クライアントの人数を1人増やす
					sprintf(new_buff,"->%sさんがloginしました ＊残り接続可能数:%d\n",buff, MAX_USER-nuser);
					if(chkerr < 0) myerror("sprintf_error");

					if(nuser == MAX_USER){	//最大人数に達した時警告文をいれる
						sprintf(buff, "%s＊＊ユーザ人数が上限になりました＊＊\n", new_buff);
						if(chkerr < 0) myerror("sprintf_error");
						strcpy(new_buff, buff);
					}
					for(i=0;i<MAX_USER;i++){	//全クライアントに送信する
						if(user[i].login){
							chkerr = write(user[i].new_sockfd[0].fd, new_buff, 128);
							if(chkerr < 0) myerror("write_comment_error");
							if(i != j){
								chkerr = sprintf(buff,"->%sさんがloginしています\n",user[i].name);
								if(chkerr < 0) myerror("sprintf_error");
								chkerr = write(user[j].new_sockfd[0].fd, buff, 128);
								if(chkerr < 0) myerror("write_comment_error");
							}
						}
					}
					printf("%s",new_buff);
					break;
				}
			}
		}

		for(j=0;j<MAX_USER;j++){
			poll(user[j].new_sockfd,1,0);

			if(user[j].new_sockfd[0].revents == POLLIN  &&  user[j].login == 1){
			//クライアントからの通信要求がある場合に入る。ログイン状態でない場合は入らない
				chkerr = read(user[j].new_sockfd[0].fd, buff, 128);	//データの受け取り
				if(chkerr < 0){
					myerror("read_comment_error");
				}else if(chkerr == 0){
				//chkerrが0の場合、"LOGOUT"入力以外の方法でクライアントとのソケットが切れた場合入る
					strcpy(buff, "LOGOUT");		//LOGOUT扱いにする
				}

				if(strcmp(buff, "LOGOUT") == 0){	//"LOGOUT"が入力された時
					chkerr = write(user[j].new_sockfd[0].fd, buff, 128);	//クライアントに"LOGOUT"を送りソケットをcloseさせる
					if(chkerr < 0) myerror("write_error");

					nuser--;	//クライアントを1人減らす
					user[j].login = 0;	//ログアウト状態にする

					chkerr = sprintf(new_buff, "<-%sさんがlogoutしました ＊残り接続可能数:%d\n", user[j].name, MAX_USER-nuser);
					if(chkerr < 0) myerror("sprintf_error");
				}else{	//それ以外のコメントが送られた場合
					chkerr = sprintf(new_buff, "%s: %s\n", user[j].name, buff);
					if(chkerr < 0) myerror("sprintf_error");
				}
 				printf("%s",new_buff);

				for(i=0;i<MAX_USER;i++){	//全クライアントに送信する
					if(user[i].login){
						chkerr = write(user[i].new_sockfd[0].fd, new_buff, 128);
        				if(chkerr < 0) myerror("write_comment_error");
					}
				}
			}
		}
		if(fds[1].revents == POLLIN){	//キーボードからの入力がある場合に入る
			scanf("%s",buff);
			if(strcmp(buff, "SERVER_END") == 0){	//"SERVER_END"から入力された場合while文から抜ける
				break;
			}
			else{
				printf("無効なコマンドです\n");
			}
		}
	}
	for(i=0;i<MAX_USER;i++){	//全クライアントにソケットをcloseするよう送信する
		if(user[i].login){
			chkerr = write(user[i].new_sockfd[0].fd, "SERVER_END", 128);
			if(chkerr < 0) myerror("write_serverend_error");
		}
	}


	//全クライアントのソケットcloseするまで待つ
	for(i=0;i<MAX_USER;i++){
		if(user[i].login){
			while(1){
				chkerr = read(user[i].new_sockfd[0].fd, buff, 128);
				if(chkerr < 0){
					myerror("close_read_error");
				}else if(chkerr == 0){
					//chkerrの値が0ならばソケットがcloseされている
					break;
				}
			}
		}
	}

	printf("サーバーを終了します\n");

	//ソケットを終了する
	chkerr = close(sockfd);
	if(chkerr < 0) myerror("close_sockfd");
}
