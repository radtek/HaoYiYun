<?php

class RecordViewModel extends ViewModel {
    public $viewFields = array(
        'Record'=>array('record_id','camera_id','gather_id','image_id','file_src','file_fdfs','file_size','duration','disptime','clicks','created','updated','_type'=>'LEFT'), //Must be LEFT 返回左表所有数据，即使右表没有匹配的
        'Camera'=>array('camera_id','camera_name','_on'=>'Camera.camera_id=Record.camera_id','_type'=>'LEFT'), //Must be LEFT
        'Image'=>array('image_id','file_fdfs'=>'image_fdfs','_on'=>'Image.image_id=Record.image_id'),
    );
}
?>