/** <module> Utilities for handling OWL information in KnowRob.

  Copyright (C) 2011 Moritz Tenorth, 2016 Daniel Beßler
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:
      * Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.
      * Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.
      * Neither the name of the <organization> nor the
        names of its contributors may be used to endorse or promote products
        derived from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

@author Moritz Tenorth, Daniel Beßler
@license BSD

*/

:- module(knowrob_owl,
    [
      owl_instance_of/2,
      owl_triple/3,             % ?Subject, ?Predicate, ?Object
      owl_class_properties/3,
      owl_class_properties_some/3,
      owl_class_properties_all/3,
      owl_class_properties_value/3,
      owl_class_properties_nosup/3,
      owl_class_properties_transitive_nosup/3,
      owl_inspect/3,
      owl_write_readable/1,
      owl_readable/2,
      owl_computable_db/1,
      knowrob_instance_from_class/2,
      knowrob_instance_from_class/3
    ]).

:- use_module(library('semweb/rdf_db')).
:- use_module(library('semweb/rdfs')).
:- use_module(library('semweb/owl')).
:- use_module(library('knowrob/computable')).
:- use_module(library('knowrob/rdfs')).
:- use_module(library('knowrob/transforms')).

:- rdf_meta owl_instance_of(r, t),
            owl_triple(r, r, o),
            owl_class_properties(r,r,t),
            owl_class_properties_some(r,r,t),
            owl_class_properties_all(r,r,t),
            owl_class_properties_value(r,r,t),
            owl_class_properties_nosup(r,r,r),
            owl_class_properties_transitive_nosup(r,r,r),
            owl_inspect(r,r,r),
            owl_readable(r,-),
            owl_write_readable(r),
            owl_computable_db(-),
            knowrob_instance_from_class(r,-),
            knowrob_instance_from_class(r,t,-).

% define holds as meta-predicate and allow the definitions
% to be in different source files
:- meta_predicate knowrob_instance_from_class(0, ?, ?, ?).
:- multifile knowrob_instance_from_class/3.

:- rdf_db:rdf_register_ns(knowrob,'http://knowrob.org/kb/knowrob.owl#', [keep(true)]).

		 /*******************************
		 *		  ABOX reasoning		*
		 *******************************/

%% owl_computable_db
owl_computable_db(db(rdfs_computable_has,rdfs_instance_of)).

%%  owl_instance_of(?Resource, +Description) is nondet.
%
% Test  or  generate  the  resources    that  satisfy  Description
% according the the OWL-Description entailment rules.

owl_instance_of(Resource, Description) :-
  owl_computable_db(DB),
  owl_individual_of(Resource, Description, DB).

%%	owl_has(?Subject, ?Predicate, ?Object)
%
%	True if this relation is specified or can be deduced using OWL
%	inference rules.  It adds transitivity to owl_has_direct/3.

owl_triple(S, P, O) :-
  owl_computable_db(DB),
  owl_has(S, P, O, DB).

		 /*******************************
		 *		  TBOX reasoning		*
		 *******************************/

%% owl_class_properties(?Class, ?Prop, ?Val) is nondet.
%
% Collect all property values of someValuesFrom- and hasValue-restrictions of a class
%
% @param Class Class whose restrictions are being considered
% @param Prop  Property whose restrictions in Class are being considered
% @param Val   Values that appear in a restriction of a superclass of Class on Property
%
owl_class_properties(Class, Prop, Val) :-
  rdfs_individual_of(Class, owl:'Class'), % make sure Class is bound before calling owl_subclass_of
  (owl_class_properties_some(Class, Prop, Val);
   owl_class_properties_value(Class, Prop, Val)).

%% owl_class_properties_some(?Class, ?Prop, ?Val) is nondet.
%
% Collect all property values of someValuesFrom-restrictions of a class
%
% @param Class Class whose restrictions are being considered
% @param Prop  Property whose restrictions in Class are being considered
% @param Val   Values that appear in a restriction of a superclass of Class on Property
%
owl_class_properties_some(Class, Prop, Val) :-         % read directly asserted properties
  owl_class_properties_1_some(Class, Prop, Val).

owl_class_properties_some(Class, Prop, Val) :-         % also consider properties of superclasses
  owl_subclass_of(Class, Super), Class\=Super,
  owl_class_properties_1_some(Super, Prop, Val).

owl_class_properties_1_some(Class, Prop, Val) :-       % read all values for some_values_from restrictions

  ( (nonvar(Class)) -> (owl_direct_subclass_of(Class, Sup)) ; (Sup     = Class)),
  ( (nonvar(Prop))  -> (rdfs_subproperty_of(SubProp, Prop)) ; (SubProp = Prop)),

  owl_restriction(Sup,restriction(SubProp, some_values_from(Val))).

%% owl_class_properties_all(?Class, ?Prop, ?Val) is nondet.
%
% Collect all property values of allValuesFrom-restrictions of a class
%
% @param Class Class whose restrictions are being considered
% @param Prop  Property whose restrictions in Class are being considered
% @param Val   Values that appear in a restriction of a superclass of Class on Property
%
owl_class_properties_all(Class, Prop, Val) :-         % read directly asserted properties
  owl_class_properties_1_all(Class, Prop, Val).

owl_class_properties_all(Class, Prop, Val) :-         % also consider properties of superclasses
  owl_subclass_of(Class, Super), Class\=Super,
  owl_class_properties_1_all(Super, Prop, Val).

owl_class_properties_1_all(Class, Prop, Val) :-       % read all values for all_values_from restrictions

  ( (nonvar(Class)) -> (owl_direct_subclass_of(Class, Sup)) ; (Sup     = Class)),
  ( (nonvar(Prop))  -> (rdfs_subproperty_of(SubProp, Prop)) ; (SubProp = Prop)),

  owl_restriction(Sup,restriction(SubProp, all_values_from(Val))) .

%% owl_class_properties_value(?Class, ?Prop, ?Val) is nondet.
%
% Collect all property values of hasValue-restrictions of a class
%
% @param Class Class whose restrictions are being considered
% @param Prop  Property whose restrictions in Class are being considered
% @param Val   Values that appear in a restriction of a superclass of Class on Property
%
owl_class_properties_value(Class, Prop, Val) :-         % read directly asserted properties
  owl_class_properties_1_value(Class, Prop, Val).

owl_class_properties_value(Class, Prop, Val) :-         % also consider properties of superclasses
  owl_subclass_of(Class, Super), Class\=Super,
  owl_class_properties_1_value(Super, Prop, Val).

owl_class_properties_1_value(Class, Prop, Val) :-       % read all values for has_value restrictions

  ( (nonvar(Class)) -> (owl_direct_subclass_of(Class, Sup)) ; (Sup     = Class)),
  ( (nonvar(Prop))  -> (rdfs_subproperty_of(SubProp, Prop)) ; (SubProp = Prop)),

  owl_restriction(Sup,restriction(SubProp, has_value(Val))) .

%% owl_class_properties_nosup(?Class, ?Prop, ?Val) is nondet.
%
% Version of owl_class_properties without considering super classes
%
% @param Class Class whose restrictions are being considered
% @param Prop  Property whose restrictions in Class are being considered
% @param Val   Values that appear in a restriction of a superclass of Class on Property
%
owl_class_properties_nosup(Class, Prop, Val) :-         % read directly asserted properties
  owl_class_properties_nosup_1(Class, Prop, Val).

% owl_class_properties_nosup(Class, Prop, Val) :-         % do not consider properties of superclasses
%   owl_subclass_of(Class, Super), Class\=Super,
%   owl_class_properties_nosup_1(Super, Prop, Val).

owl_class_properties_nosup_1(Class, Prop, Val) :-
  owl_direct_subclass_of(Class, Sup),
  ( (nonvar(Prop)) -> (rdfs_subproperty_of(SubProp, Prop)) ; (SubProp = Prop)),

  ( owl_restriction(Sup,restriction(SubProp, some_values_from(Val))) ;
    owl_restriction(Sup,restriction(SubProp, has_value(Val))) ).

%% owl_class_properties_transitive_nosup(?Class, ?Prop, ?Val) is nondet.
%
% Transitive cersion of owl_class_properties without considering super classes
%
% @param Class Class whose restrictions are being considered
% @param Prop  Property whose restrictions in Class are being considered
% @param Val   Values that appear in a restriction of a superclass of Class on Property
%
owl_class_properties_transitive_nosup(Class, Prop, SubComp) :-
    owl_class_properties_nosup(Class, Prop, SubComp).
owl_class_properties_transitive_nosup(Class, Prop, SubComp) :-
    owl_class_properties_nosup(Class, Prop, Sub),
    owl_individual_of(Prop, owl:'TransitiveProperty'),
    Sub \= Class,
    owl_class_properties_transitive_nosup(Sub, Prop, SubComp).

%% owl_inspect(?Thing, ?P, ?O).
%
% Read information stored for Thing. Basically a wrapper around owl_has and
% owl_class_properties that aggregates their results. The normal use case is to
% have Thing bound and ask for all property/object pairs for this class or
% individual.
%
% @param Thing  RDF identifier, either an OWL class or an individual
% @param P      OWL property identifier 
% @param O      OWL class, individual or literal value specified as property P of Thing
% 
owl_inspect(Thing, P, O) :-
  rdf_has(Thing, rdf:type, owl:'NamedIndividual'),
  owl_has(Thing, P, Olit),
  strip_literal_type(Olit, O).
  
owl_inspect(Thing, P, O) :-
  rdf_has(Thing, rdf:type, owl:'Class'),
  owl_class_properties(Thing, P, Olit),
  strip_literal_type(Olit, O).

		 /*******************************
		 *		  Input-Output			*
		 *******************************/

%% owl_write_readable(+Resource) is semidet
% 
% Writes human readable description of Resource.
%
% @param Resource OWL resource
%
owl_write_readable(Resource) :- owl_readable(Resource,Readable), write(Readable).

%% owl_readable(+Resource, -Readable).
%
% Utility predicate to convert RDF terms into a readable representation.
%
owl_readable(class(Cls),Out) :- owl_readable_internal(Cls,Out), !.
owl_readable(Descr,Out) :-
  (is_list(Descr) -> X=Descr ; Descr=..X),
  findall(Y_, (member(X_,X), once(owl_readable_internal(X_,Y_))), Y),
  Out=Y.
owl_readable_internal(P,P_readable) :-
  atom(P), rdf_has(P, owl:inverseOf, P_inv),
  owl_readable_internal(P_inv, P_inv_),
  atomic_list_concat(['inverse_of(',P_inv_,')'], '', P_readable), !.
owl_readable_internal(class(X),Y) :- owl_readable_internal(X,Y).
owl_readable_internal(X,Y) :- atom(X), rdf_split_url(_, Y, X).
owl_readable_internal(X,X) :- atom(X).
owl_readable_internal(X,Y) :- compound(X), rdf_readable(X,Y).

		 /*******************************
		 *		  ABOX ASSERTIONS		*
		 *******************************/

knowrob_instance_from_class(Class, Instance) :-
  ( knowrob_instance_from_class(Class, [], Instance);
    rdf_instance_from_class(Class, Instance)), !.

%%%%%%%%%%%%%%%%%%%
%% knowrob:TimePoint

knowrob_instance_from_class('http://knowrob.org/kb/knowrob.owl#TimePoint', [], TimePoint) :- !,
  current_time(T),
  knowrob_instance_from_class('http://knowrob.org/kb/knowrob.owl#TimePoint', [instant=T], TimePoint).

knowrob_instance_from_class('http://knowrob.org/kb/knowrob.owl#TimePoint', [instant=T], TimePoint) :- !,
  time_term(T,T_value),
  atom_concat('http://knowrob.org/kb/knowrob.owl#timepoint_', T_value, TimePoint),
  rdf_assert(TimePoint, rdf:type, knowrob:'TimePoint').
  
%%%%%%%%%%%%%%%%%%%
%% knowrob:TimeInterval

knowrob_instance_from_class('http://knowrob.org/kb/knowrob.owl#TimeInterval', [begin=Start], TimeInterval) :- !,
  time_term(Start, Start_v), 
  knowrob_instance_from_class(knowrob:'TimePoint', [instant=Start_v], StartI),
  atomic_list_concat(['http://knowrob.org/kb/knowrob.owl#TimeInterval',Start_v], '_', TimeInterval),
  rdf_assert(TimeInterval, rdf:type, knowrob:'TimeInterval'),
  rdf_assert(TimeInterval, knowrob:'startTime', StartI).

knowrob_instance_from_class('http://knowrob.org/kb/knowrob.owl#TimeInterval', [begin=Start,end=End], TimeInterval) :- !,
  time_term(Start, Start_v), time_term(End, End_v), 
  knowrob_instance_from_class(knowrob:'TimePoint', [instant=Start_v], StartI),
  knowrob_instance_from_class(knowrob:'TimePoint', [instant=End_v], EndI),
  atomic_list_concat(['http://knowrob.org/kb/knowrob.owl#TimeInterval',Start_v,End_v], '_', TimeInterval),
  rdf_assert(TimeInterval, rdf:type, knowrob:'TimeInterval'),
  rdf_assert(TimeInterval, knowrob:'startTime', StartI),
  rdf_assert(TimeInterval, knowrob:'endTime', EndI).

knowrob_instance_from_class('http://knowrob.org/kb/knowrob.owl#TimeInterval', I, TimeInterval) :-
  number(I), !,
  knowrob_instance_from_class(knowrob:'TimeInterval', [begin=I], TimeInterval).

knowrob_instance_from_class('http://knowrob.org/kb/knowrob.owl#TimeInterval', I, TimeInterval) :-
  interval(I, [Start,End]), !,
  knowrob_instance_from_class(knowrob:'TimeInterval', [begin=Start,end=End], TimeInterval).

knowrob_instance_from_class('http://knowrob.org/kb/knowrob.owl#TimeInterval', I, TimeInterval) :-
  interval(I, [Start]), !,
  knowrob_instance_from_class(knowrob:'TimeInterval', [begin=Start], TimeInterval).

%%%%%%%%%%%%%%%%%%%
%% knowrob:Pose

knowrob_instance_from_class('http://knowrob.org/kb/knowrob.owl#Pose', [pose=(Frame,[X,Y,Z],[QW,QX,QY,QZ])], Pose) :- !,
  rdfs_individual_of(Frame, knowrob:'SpatialThing-Localized'),
  rdf_split_url(_, Ref, Frame),
  atomic_list_concat(['http://knowrob.org/kb/knowrob.owl#Pose'|[Ref,X,Y,Z,QW,QX,QY,QZ]], '_', Pose),
  atomic_list_concat([X,Y,Z], ' ', Translation),
  atomic_list_concat([QW,QX,QY,QZ], ' ', Quaternion),
  rdf_assert(Pose, rdf:type, knowrob:'Pose'),
  rdf_assert(Pose, knowrob:'translation', literal(type(string,Translation))),
  rdf_assert(Pose, knowrob:'quaternion', literal(type(string,Quaternion))),
  rdf_assert(Pose, knowrob:'relativeTo', Frame).

knowrob_instance_from_class('http://knowrob.org/kb/knowrob.owl#Pose', [pose=(Pos,Rot)], Pose) :- !,
  knowrob_instance_from_class(knowrob:'Pose',
      [pose=(knowrob:'MapFrame', Pos, Rot)], Pose).

knowrob_instance_from_class('http://knowrob.org/kb/knowrob.owl#Pose', [mat=(Data)], Pose) :- !,
  matrix_translation(Data, Pos),
  matrix_rotation(Data, Rot),
  knowrob_instance_from_class('http://knowrob.org/kb/knowrob.owl#Pose', [pose=(Pos, Rot)], Pose).

knowrob_instance_from_class('http://knowrob.org/kb/knowrob.owl#Pose', [pose=pose(A,B,C)], Pose) :- !,
  knowrob_instance_from_class('http://knowrob.org/kb/knowrob.owl#Pose', [pose=(A,B,C)], Pose).

knowrob_instance_from_class('http://knowrob.org/kb/knowrob.owl#Pose', [pose=pose(A,B)], Pose) :- !,
  knowrob_instance_from_class('http://knowrob.org/kb/knowrob.owl#Pose', [pose=(A,B)], Pose).

%%%%%%%%%%%%%%%%%%%
%% knowrob:'FrameOfReference'

knowrob_instance_from_class('http://knowrob.org/kb/knowrob.owl#FrameOfReference', [urdf=Name], Frame) :- !,
  atomic_list_concat(['http://knowrob.org/kb/knowrob.owl#FrameOfReference'|[Name]], '_', Frame),
  rdf_assert(Frame, rdf:type, knowrob:'FrameOfReference'),
  rdf_assert(Frame, 'http://knowrob.org/kb/srdl2-comp.owl#urdfName', literal(type(xsd:string, Name))).

%%%%%%%%%%%%%%%%%%%
%% knowrob:'SpaceRegion'

knowrob_instance_from_class('http://knowrob.org/kb/knowrob.owl#SpaceRegion', [axioms=Axioms], SpaceRegion) :- !,
  location_name_args_(Axioms,Args),
  atomic_list_concat(['http://knowrob.org/kb/knowrob.owl#SpaceRegion'|Args], '_', SpaceRegion),
  rdf_assert(SpaceRegion, rdf:type, knowrob:'SpaceRegion'),
  forall( member([P,O], Axioms), rdf_assert(SpaceRegion, P, O) ).

location_name_args_([[P,O]|Axioms], [P_name|[O_name|Args]]) :-
  rdf_split_url(_, P_name, P),
  rdf_split_url(_, O_name, O),
  location_name_args_(Axioms, Args).
location_name_args_([], []).
