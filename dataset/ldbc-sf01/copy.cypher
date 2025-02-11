COPY Comment FROM "dataset/ldbc-sf01/Comment.csv" ;
COPY Forum FROM "dataset/ldbc-sf01/Forum.csv" ;
COPY Organisation FROM "dataset/ldbc-sf01/Organisation.csv" ;
COPY Person FROM "dataset/ldbc-sf01/Person.csv" ;
COPY Place FROM "dataset/ldbc-sf01/Place.csv" ;
COPY Post FROM "dataset/ldbc-sf01/Post.csv" ;
COPY Tag FROM "dataset/ldbc-sf01/Tag.csv" ;
COPY TagClass FROM "dataset/ldbc-sf01/TagClass.csv" ;
COPY Comment_hasCreator FROM "dataset/ldbc-sf01/Comment_hasCreator_Person.csv" ;
COPY Comment_hasCreator FROM "dataset/ldbc-sf01/Comment_hasCreator_Person_1.csv";
COPY Comment_hasTag FROM "dataset/ldbc-sf01/Comment_hasTag_Tag.csv" ;
COPY Comment_hasTag FROM "dataset/ldbc-sf01/Comment_hasTag_Tag_1.csv";
COPY Comment_isLocatedIn FROM "dataset/ldbc-sf01/Comment_isLocatedIn_Place.csv" ;
COPY Comment_isLocatedIn FROM "dataset/ldbc-sf01/Comment_isLocatedIn_Place_1.csv";
COPY replyOf_Comment FROM "dataset/ldbc-sf01/Comment_replyOf_Comment.csv" ;
COPY replyOf_Post FROM "dataset/ldbc-sf01/Comment_replyOf_Post.csv" ;
COPY containerOf FROM "dataset/ldbc-sf01/Forum_containerOf_Post.csv" ;
COPY hasMember FROM "dataset/ldbc-sf01/Forum_hasMember_Person.csv" ;
COPY hasModerator FROM "dataset/ldbc-sf01/Forum_hasModerator_Person.csv" ;
COPY Forum_hasTag FROM "dataset/ldbc-sf01/Forum_hasTag_Tag.csv" ;
COPY Organisation_isLocatedIn FROM "dataset/ldbc-sf01/Organisation_isLocatedIn_Place.csv" ;
COPY hasInterest FROM "dataset/ldbc-sf01/Person_hasInterest_Tag.csv" ;
COPY Person_isLocatedIn FROM "dataset/ldbc-sf01/Person_isLocatedIn_Place.csv" ;
COPY knows FROM "dataset/ldbc-sf01/Person_knows_Person.csv" ;
COPY knows FROM "dataset/ldbc-sf01/Person_knows_Person_1.csv" ;
COPY likes_Comment FROM "dataset/ldbc-sf01/Person_likes_Comment.csv" ;
COPY likes_Comment FROM "dataset/ldbc-sf01/Person_likes_Comment_1.csv" ;
COPY likes_Post FROM "dataset/ldbc-sf01/Person_likes_Post.csv" ;
COPY likes_Post FROM "dataset/ldbc-sf01/Person_likes_Post_1.csv";
COPY studyAt FROM "dataset/ldbc-sf01/Person_studyAt_Organisation.csv" ;
COPY workAt FROM "dataset/ldbc-sf01/Person_workAt_Organisation.csv" ;
COPY isPartOf FROM "dataset/ldbc-sf01/Place_isPartOf_Place.csv" ;
COPY Post_hasCreator FROM "dataset/ldbc-sf01/Post_hasCreator_Person.csv" ;
COPY Post_hasTag FROM "dataset/ldbc-sf01/Post_hasTag_Tag.csv" ;
COPY Post_isLocatedIn FROM "dataset/ldbc-sf01/Post_isLocatedIn_Place.csv" ;
COPY hasType FROM "dataset/ldbc-sf01/Tag_hasType_TagClass.csv" ;
COPY isSubclassOf FROM "dataset/ldbc-sf01/TagClass_isSubclassOf_TagClass.csv" ;
