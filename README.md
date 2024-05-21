# Device mapper proxy.

Модуль тестировался на Arch Linux с ядром версии 6.9.1. Для сборки модуля требуется `gcc` версии 5 и более и GNU Make.

## Компиляция и загрузка модуля
### После $ идёт команда обычного пользователя, после # идёт команда root-пользователя
1. Склонируйте репозиторий и в нём выполните
```bash
$ make
```
2. Загрузите модуль
```bash
# insmod dmp.ko
```
3. Чтобы проверить, что модуль загружен можно выполнить
```bash
$ lsmod | grep dmp
```
Если модуль `dmp` найден, то он успешно загружен.

## Тестирование модуля.

Создание тестового блочного устройства

```bash
# dmsetup create zero1 --table "0 1024 zero"
```
1024 - размер $size

Устройство успешно создалось
```bash
$ ls -al /dev/mapper/*
```
`lrwxrwxrwx 1 root root 21 Мая 20 10:12 /dev/mapper/zero1 -> ../dm-0`

Создание нашего device mapper proxy
```bash
# dmsetup create dmp1 --table "0 1024 dmp /dev/mapper/zero1"
```
Note: $size - размер устройства /dev/mapper/zero1.

Устройство успешно создалось
```
$ ls -al /dev/mapper/*
lrwxrwxrwx 1 root root 21 Мая 20 10:12 /dev/mapper/zero1 -> ../dm-0
lrwxrwxrwx 1 root root 21 Мая 20 10:12 /dev/mapper/dmp1 -> ../dm-1
```

Операции на запись и чтение
```
# dd if=/dev/random of=/dev/mapper/dmp1 bs=1024 count=1
1+0 records in
1+0 records out
1024 bytes (1.0 kB, 1.0 KiB) copied, 0.000155393 s, 6.6 MB/s
# dd of=/dev/null if=/dev/mapper/dmp1 bs=1024 count=1
1+0 records in
1+0 records out
1024 bytes (1.0 kB, 1.0 KiB) copied, 0.000366171 s, 2.8 MB/s
```

Статистика:

```cat /sys/module/dmp/stat/volumes
read:
 reqs: 47
 avg size: 6400
write:
 reqs: 1
 avg size: 4096
total:
 reqs: 48
 avg size: 6400
```
