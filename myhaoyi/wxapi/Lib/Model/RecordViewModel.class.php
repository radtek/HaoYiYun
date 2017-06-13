<?php

class RecordViewModel extends ViewModel {
    public $viewFields = array(
        'Record'=>array('record_id','camera_id','subject_id','teacher_id','image_id','file_fdfs','duration','disptime','clicks','created','updated','_type'=>'LEFT'), //Must be LEFT 返回左表所有数据，即使右表没有匹配的
        'Teacher'=>array('teacher_id','teacher_name','sex_name','title_name','_on'=>'Teacher.teacher_id=Record.teacher_id','_type'=>'LEFT'), //Must be LEFT
        'Camera'=>array('camera_id','grade_id','camera_name','_on'=>'Camera.camera_id=Record.camera_id','_type'=>'LEFT'), //Must be LEFT
        'Grade'=>array('grade_id','grade_type','grade_name','_on'=>'Grade.grade_id=Camera.grade_id','_type'=>'LEFT'), //Must be LEFT
        'Image'=>array('image_id','file_fdfs'=>'image_fdfs','_on'=>'Image.image_id=Record.image_id','_type'=>'LEFT'), //Must be LEFT
        'Subject'=>array('subject_id','subject_name','_on'=>'Subject.subject_id=Record.subject_id'),
    );
}
?>