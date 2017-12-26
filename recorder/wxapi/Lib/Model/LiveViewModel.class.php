<?php

class LiveViewModel extends ViewModel {
    public $viewFields = array(
        'Camera'=>array('*','_type'=>'LEFT'), //Must be LEFT 返回左表所有数据，即使右表没有匹配的
        'Gather'=>array('gather_id','school_id','mac_addr','ip_addr','name_pc','name_set','_on'=>'Gather.gather_id=Camera.gather_id','_type'=>'LEFT'),
        'School'=>array('school_id','name'=>'school_name','addr'=>'school_addr','_on'=>'Gather.school_id=School.school_id','_type'=>'LEFT'),
        'Grade'=>array('grade_id','grade_type','grade_name','_on'=>'Grade.grade_id=Camera.grade_id','_type'=>'LEFT'),
        'Image'=>array('image_id','file_fdfs'=>'image_fdfs','_on'=>'Image.image_id=Camera.image_id'),
    );
}
?>