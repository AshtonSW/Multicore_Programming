<Multicore Programming Project 1: Myshell>

Phase 2 : redirection and piping in your shell


1. 컴파일 방법
4 파일이 모두 포함된 디렉토리에서 make command를 입력한다.
myshell.c
csapp.h
csapp.c
Makefile

2. 실행 방법
컴파일 완료 후 ./myshell을 입력하여 프로그램을 실행시킨다.
프로그램이 실행되면 CSE4100-MP-P1>이 출력된다.
원하는 명령어를 입력하여 사용한다.

<phase 1>
cd : 디렉토리 탐색 및 이동
ls : 디렉토리 및 파일 목록 출력
mkdir : 디렉토리 생성
rmdir : 디렉토리 삭제
touch : 파일 생성 명령어
cat : 파일 내용 읽기
echo : 파일 내용 출력
exit : 쉘 종료
quit : 쉘 종료

<phase 2>
Redirection과 pipe(‘|’) 기능을 추가했다.
Pipeline에 있는 각 명령어들을 위한 New process를 생성하고,
parent process가 마지막 command를 기다리도록 했다.
이때 dup()와 dup2() syscall을 이용해서 한 프로세스의 출력을 
다른 프로세스 입력으로 전달되는 기능을 이용하였다.

command example)
ls -al | grep filename
cat filename | less
cat filename | grep -v “abc” | sort – r 