# jsk_2020_04_carry_dish
This is a repository for one week project. A robot carries dishes
## Attention
I referred to B4 student repositories, thank you. I'm a beginner, so if you have some advise, please tell me!
## Start now
ROS 環境で EusLisp をインストールする.
```
$ sudo apt-get install ros-melodic-jskeus
```
また,実行ファイルへのパス (PATH) や, EusLisp 言語に必要な環境変数 (EUSDIR, ARCHDIR) 等をセットする。
```
$ source /opt/ros/melodic/setup.bash
```


## demoの流れ
- 皿を認識
- 皿を掴む
- 棚に皿を運ぶ
- 棚に置く


## additional future task
掴むのを失敗したとき(失敗しそうなとき)
- 壁を使いながら皿を持ち上げる。
- 片方の腕で失敗した際にもう片方の腕で支える。
- 落ちそうになった時に安全な位置に戻す。
- 人の声で割り込みの制御を行う。
  - 人の声をトリガーとして、緊急停止ではなくお皿を安全な位置に置こうとする行動に切り替える。
  - 人の声で簡単な指示を出して、行動決定の参考とする。


## Example

- irt-viewerのdemo
```
roseus irt-demo.l
```

- kinematics simulatorのdemo
```
roseus kinematics-demo.l
```

- gazeboで皿を掴むdemo
```
roslaunch common.launch
roseus pr2-tabletop-object-grasp-dual-success.l
```

  - common.launch 
    - pr2_tabletop_scene.launch
      - empty_world.launch
      - tabletop.world
    - pr2_tabletop.launch
      - tabletop_object_detector.launch

